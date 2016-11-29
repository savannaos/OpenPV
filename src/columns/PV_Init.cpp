/*
 * PV_Init.cpp
 *
 *  Created on: Jul 31, 2015
 *      Author: slundquist
 */
#include <cMakeHeader.h>
#include <csignal>
#ifdef PV_USE_OPENMP_THREADS
#include <omp.h>
#endif // PV_USE_OPENMP_THREADS
#include "PV_Init.hpp"
#include "columns/CommandLineArguments.hpp"
#include "columns/ConfigFileArguments.hpp"
#include "columns/HyPerCol.hpp"
#include "utils/PVLog.hpp"

namespace PV {

PV_Init::PV_Init(int *argc, char **argv[], bool allowUnrecognizedArguments) {
   // Initialize MPI
   initSignalHandler();
   commInit(argc, argv);
   initMaxThreads();
   mArgC = *argc;
   mArgV = (char **)pvMallocError((size_t)(mArgC + 1) * sizeof(char *), "PV_Init failed to allocate memory for %d arguments: %s\n", mArgC, strerror(errno));
   for (int a=0; a<mArgC; a++) {
      mArgV[a] = strdup(argv[0][a]);
      FatalIf(mArgV[a]==nullptr, "PV_Init unable to copy argument %d: %s\n", a, strerror(errno));
   }
   mArgV[mArgC]  = nullptr;
   params        = nullptr;
   mCommunicator = nullptr;
   if (mArgC >= 2 && mArgV[1] != nullptr && mArgV[1][0] != '-') {
      // Communicator doesn't get set until call to initialize(), which we can't call until rows, columns, etc.
      // are set. We therefore need to use MPI_COMM_WORLD as the MPI communicator.
      arguments = new ConfigFileArguments(std::string{mArgV[1]}, MPI_COMM_WORLD, allowUnrecognizedArguments);
   }
   else {
      arguments = new CommandLineArguments(mArgC, mArgV, allowUnrecognizedArguments);
   }
   initLogFile(false /*appendFlag*/);
   initialize(); // must be called after initialization of arguments data member.
}

PV_Init::~PV_Init() {
   for (int a=0; a<mArgC; a++) {
      free(mArgV[a]);
   }
   free(mArgV);
   delete params;
   delete mCommunicator;
   delete arguments;
   commFinalize();
}

int PV_Init::initSignalHandler() {
   // Block SIGUSR1.  root process checks for SIGUSR1 during advanceTime() and
   // broadcasts sends to all processes,
   // which saves the result in the checkpointSignal member variable.
   // When run() checks whether to call checkpointWrite, it looks at
   // checkpointSignal, and writes a
   // checkpoint if checkpointWriteFlag is true, regardless of whether the next
   // scheduled checkpoint time has arrived.
   //
   // This routine must be called before MPI_Initialize; otherwise a thread
   // created by MPI will not get the signal handler
   // but will get the signal and the job will terminate.
   sigset_t blockusr1;
   sigemptyset(&blockusr1);
   sigaddset(&blockusr1, SIGUSR1);
   sigprocmask(SIG_BLOCK, &blockusr1, NULL);
   return 0;
}

int PV_Init::initialize() {
   delete mCommunicator;
   mCommunicator = new Communicator(arguments);
   int status    = PV_SUCCESS;
   // It is okay to initialize without there being a params file.
   // setParams() can be called later.
   delete params;
   params = NULL;
   if (!arguments->getParamsFile().empty()) {
      status = createParams();
   }
   printInitMessage();
   return status;
}

int PV_Init::initMaxThreads() {
#ifdef PV_USE_OPENMP_THREADS
   maxThreads = omp_get_max_threads();
#else // PV_USE_OPENMP_THREADS
   maxThreads = 1;
#endif // PV_USE_OPENMP_THREADS
   return PV_SUCCESS;
}

int PV_Init::commInit(int *argc, char ***argv) {
   int mpiInit;
   // If MPI wasn't initialized, initialize it.
   // Remember if it was initialized on entry; the destructor will only finalize
   // if the constructor
   // init'ed.
   // This way, you can do several simulations sequentially by initializing MPI
   // before creating
   // the first HyPerCol; after running the first simulation the MPI environment
   // will still exist
   // and you
   // can run the second simulation, etc.
   MPI_Initialized(&mpiInit);
   if (!mpiInit) {
      pvAssert((*argv)[*argc] == NULL); // Open MPI 1.7 assumes this.
      MPI_Init(argc, argv);
   }
   else {
      Fatal() << "PV_Init communicator already initialized\n";
   }

   return 0;
}

void PV_Init::initLogFile(bool appendFlag) {
   // TODO: Under MPI, non-root processes should send messages to root process.
   // Currently, if logFile is directory/filename.txt, the root process writes to
   // that path,
   // and nonroot processes write to directory/filename_<rank>.txt, where <rank>
   // is replaced with
   // the global rank.
   // If filename does not have an extension, _<rank> is appended.
   // Note that the global rank zero process does not insert _<rank>.  This is
   // deliberate, as the
   // nonzero ranks
   // should be MPI-ing the data to the zero rank.
   std::string const &logFile  = arguments->getLogFile();
   int const globalRootProcess = 0;
   int globalRank;
   MPI_Comm_rank(MPI_COMM_WORLD, &globalRank);
   std::ios_base::openmode mode =
         appendFlag ? std::ios_base::out | std::ios_base::app : std::ios_base::out;
   if (!logFile.empty() && globalRank != globalRootProcess) {
      // To prevent collisions caused by multiple processes opening the same file
      // for logging,
      // processes with global rank other than zero append the rank to the log
      // filename.
      // If the logfile has an extension (e.g. ".log", ".txt"), the rank is
      // appended before the
      // period separating the extension.
      std::string logFileString(logFile);
      size_t finalSlash = logFileString.rfind('/');
      size_t finalDot   = logFileString.rfind('.');
      size_t insertionPoint;
      if (finalDot == std::string::npos
          || (finalSlash != std::string::npos && finalDot < finalSlash)) {
         insertionPoint = logFileString.length();
      }
      else {
         insertionPoint = finalDot;
      }
      std::string insertion("_");
      insertion.append(std::to_string(globalRank));
      logFileString.insert(insertionPoint, insertion);
      PV::setLogFile(logFileString, mode);
   }
   else {
      PV::setLogFile(logFile, mode);
   }
}

int PV_Init::setParams(char const *params_file) {
   if (params_file == NULL) {
      return PV_FAILURE;
   }
   arguments->setParamsFile(params_file);
   initialize();
   return createParams();
}

int PV_Init::createParams() {
   std::string const &params_file = arguments->getParamsFile();
   if (!params_file.empty()) {
      delete params;
      params = new PVParams(
            params_file.c_str(),
            2 * (INITIAL_LAYER_ARRAY_SIZE + INITIAL_CONNECTION_ARRAY_SIZE),
            mCommunicator);
      return PV_SUCCESS;
   }
   else {
      return PV_FAILURE;
   }
}

int PV_Init::setLogFile(char const *log_file, bool appendFlag) {
   arguments->setLogFile(log_file);
   initLogFile(appendFlag);
   printInitMessage();
   return PV_SUCCESS;
}

int PV_Init::setMPIConfiguration(int rows, int columns, int batchWidth) {
   if (rows >= 0) {
      arguments->setNumRows(rows);
   }
   if (columns >= 0) {
      arguments->setNumColumns(columns);
   }
   if (batchWidth >= 0) {
      arguments->setBatchWidth(batchWidth);
   }
   initialize();
   return PV_SUCCESS;
}

void PV_Init::printInitMessage() {
   time_t currentTime = time(nullptr);
   InfoLog() << "PetaVision initialized at "
             << ctime(&currentTime); // string returned by ctime contains a trailing \n.
   InfoLog() << "Configuration is:\n";
   printState();
   InfoLog().printf("----------------\n");
}

int PV_Init::resetState() { arguments->resetState(); return PV_SUCCESS; }

int PV_Init::registerKeyword(char const *keyword, ObjectCreateFn creator) {
   int status = Factory::instance()->registerKeyword(keyword, creator);
   if (status != PV_SUCCESS) {
      if (getWorldRank() == 0) {
         ErrorLog().printf("PV_Init: keyword \"%s\" has already been registered.\n", keyword);
      }
   }
   return status;
}

char **PV_Init::getArgsCopy() const {
   char **argumentArray = (char **)pvMallocError((size_t)(mArgC + 1) * sizeof(char *), "PV_Init::getArgsCopy  allocate memory for %d arguments: %s\n", mArgC, strerror(errno));
   for (int a = 0; a < mArgC; a++) {
      char const *arga = mArgV[a];
      if (arga) {
         char *copied = strdup(arga);
         if (!copied) {
            ErrorLog().printf("PV_Init unable to store argument %d: %s\n", a, strerror(errno));
            Fatal().printf("Argument was \"%s\".\n", arga);
         }
         argumentArray[a] = copied;
      }
      else {
         argumentArray[a] = nullptr;
      }
   }
   argumentArray[mArgC] = nullptr;
   return argumentArray;
}

void PV_Init::freeArgs(int argc, char **argv) {
   for (int k = 0; k < argc; k++) {
      free(argv[k]);
   }
   free(argv);
}

// PV_Init::build() was marked obsolete Jul 19, 2016 and deleted Sep 27, 2016.
// Use
// hc=createHyPerCol(pv_init_ptr) instead of hc=pv_init_ptr->build().

int PV_Init::commFinalize() {
   MPI_Finalize();
   return 0;
}

} // namespace PV
