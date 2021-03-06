/*
 * HyPerCol.cpp
 *
 *  Created on: Jul 30, 2008
 *      Author: Craig Rasmussen
 */

#define TIMER_ON
#define DEFAULT_DELTA_T 1.0 // time step size (msec)

#include "HyPerCol.hpp"
#include "columns/Communicator.hpp"
#include "columns/Factory.hpp"
#include "columns/RandomSeed.hpp"
#include "io/PrintStream.hpp"
#include "io/io.hpp"
#include "normalizers/NormalizeBase.hpp"

#include <assert.h>
#include <cmath>
#include <csignal>
#include <float.h>
#include <fstream>
#include <fts.h>
#include <libgen.h>
#include <limits>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#ifdef PV_USE_CUDA
#include <map>
#endif // PV_USE_CUDA

namespace PV {

HyPerCol::HyPerCol(const char *mName, PV_Init *initObj) {
   initialize_base();
   initialize(mName, initObj);
}

HyPerCol::~HyPerCol() {
#ifdef PV_USE_CUDA
   finalizeCUDA();
#endif // PV_USE_CUDA
   PrintStream pStream(getOutputStream());
   mCheckpointer->writeTimers(pStream);
   delete mCheckpointer;
   for (auto iterator = mConnections.begin(); iterator != mConnections.end();) {
      delete *iterator;
      iterator = mConnections.erase(iterator);
   }
   for (auto iterator = mNormalizers.begin(); iterator != mNormalizers.end();) {
      delete *iterator;
      iterator = mNormalizers.erase(iterator);
   }
   for (auto iterator = mPhaseRecvTimers.begin(); iterator != mPhaseRecvTimers.end();) {
      delete *iterator;
      iterator = mPhaseRecvTimers.erase(iterator);
   }
   for (auto iterator = mLayers.begin(); iterator != mLayers.end();) {
      delete *iterator;
      iterator = mLayers.erase(iterator);
   }

   // mColProbes[i] should not be deleted; it points to an entry in mBaseProbes
   // and will
   // be deleted when mBaseProbes is deleted
   for (auto iterator = mBaseProbes.begin(); iterator != mBaseProbes.end();) {
      delete *iterator;
      iterator = mBaseProbes.erase(iterator);
   }
   mColProbes.clear();

   delete mRunTimer;
   // TODO: Change these old C strings into std::string
   free(mPrintParamsFilename);
   free(mName);
}

int HyPerCol::initialize_base() {
   // Initialize all member variables to safe values.  They will be set to their
   // actual values in
   // initialize()
   mReadyFlag                = false;
   mParamsProcessedFlag      = false;
   mNumPhases                = 0;
   mCheckpointReadFlag       = false;
   mStartTime                = 0.0;
   mStopTime                 = 0.0;
   mDeltaTime                = DEFAULT_DELTA_T;
   mWriteTimeScaleFieldnames = true;
   // Sep 26, 2016: Adaptive timestep routines and member variables have been
   // moved to
   // AdaptiveTimeScaleProbe.
   mProgressInterval   = 1.0;
   mWriteProgressToErr = false;
   mOrigStdOut         = -1;
   mOrigStdErr         = -1;
   mLayers.clear();
   mConnections.clear();
   mNormalizers.clear(); // Pretty sure these aren't necessary
   mLayerStatus          = nullptr;
   mConnectionStatus     = nullptr;
   mPrintParamsFilename  = nullptr;
   mPrintParamsStream    = nullptr;
   mLuaPrintParamsStream = nullptr;
   mNumXGlobal           = 0;
   mNumYGlobal           = 0;
   mNumBatch             = 1;
   mNumBatchGlobal       = 1;
   mOwnsCommunicator     = true;
   mParams               = nullptr;
   mCommunicator         = nullptr;
   mRunTimer             = nullptr;
   mPhaseRecvTimers.clear();
   mColProbes.clear();
   mBaseProbes.clear();
   mRandomSeed        = 0U;
   mErrorOnNotANumber = false;
   mNumThreads        = 1;
   mRecvLayerBuffer.clear();
#ifdef PV_USE_CUDA
   mCudaDevice = nullptr;
   mGpuGroupConns.clear();
#endif
   return PV_SUCCESS;
}

int HyPerCol::initialize(const char *name, PV_Init *initObj) {
   mPVInitObj    = initObj;
   mCommunicator = mPVInitObj->getCommunicator();
   mParams       = mPVInitObj->getParams();
   if (mParams == nullptr) {
      if (mCommunicator->globalCommRank() == 0) {
         ErrorLog() << "HyPerCol::initialize: params have not been set." << std::endl;
         MPI_Barrier(mCommunicator->communicator());
      }
      exit(EXIT_FAILURE);
   }
   std::string working_dir = mPVInitObj->getStringArgument("WorkingDirectory");
   working_dir             = expandLeadingTilde(working_dir);

   // Sep 27, 2016: handling --require-return has been moved to the Communicator
   // constructor.

   mName = strdup(name);
   setDescription();

   // mNumThreads will not be set, or used until HyPerCol::run.
   // This means that threading cannot happen in the initialization or
   // communicateInitInfo stages,
   // but that should not be a problem.
   char const *programName = mPVInitObj->getProgramName();

   if (columnId() == 0 && !working_dir.empty()) {
      int status = chdir(working_dir.c_str());
      if (status) {
         Fatal(chdirMessage);
         chdirMessage.printf("Unable to switch directory to \"%s\"\n", working_dir.c_str());
         chdirMessage.printf("chdir error: %s\n", strerror(errno));
      }
   }

#ifdef PV_USE_MPI // Fail if there was a parsing error, but make sure nonroot
   // processes don't kill
   // the root process before the root process reaches the syntax error
   int parsedStatus;
   int rootproc = 0;
   if (globalRank() == rootproc) {
      parsedStatus = this->mParams->getParseStatus();
   }
   MPI_Bcast(&parsedStatus, 1, MPI_INT, rootproc, getCommunicator()->globalCommunicator());
#else
   int parsedStatus                         = this->mParams->getParseStatus();
#endif
   if (parsedStatus != 0) {
      exit(parsedStatus);
   }

   mRandomSeed = mPVInitObj->getUnsignedIntArgument("RandomSeed");

   mCheckpointer = new Checkpointer(
         std::string(mName), mCommunicator->getGlobalMPIBlock(), mPVInitObj->getArguments());
   mCheckpointer->addObserver(this, BaseMessage{});
   ioParams(PARAMS_IO_READ);
   mSimTime     = mStartTime;
   mInitialStep = (long int)nearbyint(mStartTime / mDeltaTime);
   mCurrentStep = mInitialStep;
   mFinalStep   = (long int)nearbyint(mStopTime / mDeltaTime);
   mCheckpointer->provideFinalStep(mFinalStep);
   mNextProgressTime = mStartTime;

   RandomSeed::instance()->initialize(mRandomSeed);

   mRunTimer = new Timer(mName, "column", "run    ");
   mCheckpointer->registerTimer(mRunTimer);
   mCheckpointer->registerCheckpointData(
         mName, "nextProgressTime", &mNextProgressTime, (std::size_t)1, true /*broadcast*/);

   mCheckpointReadFlag = !mCheckpointer->getCheckpointReadDirectory().empty();
   return PV_SUCCESS;
}

void HyPerCol::setDescription() {
   description = "HyPerCol \"";
   description.append(getName()).append("\"");
}

int HyPerCol::ioParams(enum ParamsIOFlag ioFlag) {
   ioParamsStartGroup(ioFlag, mName);
   ioParamsFillGroup(ioFlag);
   ioParamsFinishGroup(ioFlag);
   return PV_SUCCESS;
}

int HyPerCol::ioParamsStartGroup(enum ParamsIOFlag ioFlag, const char *group_name) {
   if (ioFlag == PARAMS_IO_WRITE && mCheckpointer->getMPIBlock()->getRank() == 0) {
      pvAssert(mPrintParamsStream);
      pvAssert(mLuaPrintParamsStream);
      const char *keyword = mParams->groupKeywordFromName(group_name);
      mPrintParamsStream->printf("\n");
      mPrintParamsStream->printf("%s \"%s\" = {\n", keyword, group_name);
      mLuaPrintParamsStream->printf("%s = {\n", group_name);
      mLuaPrintParamsStream->printf("groupType = \"%s\";\n", keyword);
   }
   return PV_SUCCESS;
}

int HyPerCol::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   ioParam_startTime(ioFlag);
   ioParam_dt(ioFlag);
   ioParam_stopTime(ioFlag);
   ioParam_progressInterval(ioFlag);
   ioParam_writeProgressToErr(ioFlag);
   mCheckpointer->ioParams(ioFlag, parameters());
   ioParam_printParamsFilename(ioFlag);
   ioParam_randomSeed(ioFlag);
   ioParam_nx(ioFlag);
   ioParam_ny(ioFlag);
   ioParam_nBatch(ioFlag);
   ioParam_errorOnNotANumber(ioFlag);

   return PV_SUCCESS;
}

int HyPerCol::ioParamsFinishGroup(enum ParamsIOFlag ioFlag) {
   if (ioFlag == PARAMS_IO_WRITE && mPrintParamsStream != nullptr) {
      pvAssert(mLuaPrintParamsStream);
      mPrintParamsStream->printf("};\n");
      mLuaPrintParamsStream->printf("};\n\n");
   }
   return PV_SUCCESS;
}

// Sep 26, 2016: HyPerCol methods for parameter input/output have been moved to
// PVParams.

void HyPerCol::ioParam_startTime(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValue(ioFlag, mName, "startTime", &mStartTime, mStartTime);
}

void HyPerCol::ioParam_dt(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValue(ioFlag, mName, "dt", &mDeltaTime, mDeltaTime);
}

void HyPerCol::ioParam_stopTime(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValue(ioFlag, mName, "stopTime", &mStopTime, mStopTime);
}

void HyPerCol::ioParam_progressInterval(enum ParamsIOFlag ioFlag) {
   if (ioFlag == PARAMS_IO_READ && !mParams->present(mName, "progressInterval")
       && mParams->present(mName, "progressStep")) {
      long int progressStep = (long int)mParams->value(mName, "progressStep");
      mProgressInterval     = progressStep / mDeltaTime;
      if (globalRank() == 0) {
         ErrorLog() << "progressStep is obsolete.  Use progressInterval instead.\n";
      }
      MPI_Barrier(getCommunicator()->communicator());
      exit(EXIT_FAILURE);
   }
   // progressStep was deprecated Dec 18, 2013
   // After a reasonable fade time, remove the above if-statement and keep the
   // ioParamValue call
   // below.
   parameters()->ioParamValue(
         ioFlag, mName, "progressInterval", &mProgressInterval, mProgressInterval);
}

void HyPerCol::ioParam_writeProgressToErr(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValue(
         ioFlag, mName, "writeProgressToErr", &mWriteProgressToErr, mWriteProgressToErr);
}

void HyPerCol::ioParam_printParamsFilename(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamString(
         ioFlag, mName, "printParamsFilename", &mPrintParamsFilename, "pv.params");
   if (mPrintParamsFilename == nullptr || mPrintParamsFilename[0] == '\0') {
      if (mCheckpointer->getMPIBlock()->getRank() == 0) {
         ErrorLog().printf("printParamsFilename cannot be null or the empty string.\n");
      }
      MPI_Barrier(mCheckpointer->getMPIBlock()->getComm());
      exit(EXIT_FAILURE);
   }
}

void HyPerCol::ioParam_randomSeed(enum ParamsIOFlag ioFlag) {
   switch (ioFlag) {
      // randomSeed can be set on the command line, from the params file, or from
      // the system clock
      case PARAMS_IO_READ:
         // set random seed if it wasn't set in the command line
         if (!mRandomSeed) {
            if (mParams->present(mName, "randomSeed")) {
               mRandomSeed = (unsigned long)mParams->value(mName, "randomSeed");
            }
            else {
               mRandomSeed = seedRandomFromWallClock();
            }
         }
         if (mRandomSeed < RandomSeed::minSeed) {
            Fatal().printf(
                  "Error: random seed %u is too small. Use a seed of at "
                  "least 10000000.\n",
                  mRandomSeed);
         }
         break;
      case PARAMS_IO_WRITE: parameters()->writeParam("randomSeed", mRandomSeed); break;
      default: assert(0); break;
   }
}

void HyPerCol::ioParam_nx(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValueRequired(ioFlag, mName, "nx", &mNumXGlobal);
}

void HyPerCol::ioParam_ny(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValueRequired(ioFlag, mName, "ny", &mNumYGlobal);
}

void HyPerCol::ioParam_nBatch(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValue(ioFlag, mName, "nbatch", &mNumBatchGlobal, mNumBatchGlobal);
   // Make sure numCommBatches is a multiple of mNumBatch specified in the params
   // file
   FatalIf(
         mNumBatchGlobal % mCommunicator->numCommBatches() != 0,
         "The total number of batches (%d) must be a multiple of the batch "
         "width (%d)\n",
         mNumBatchGlobal,
         mCommunicator->numCommBatches());
   mNumBatch = mNumBatchGlobal / mCommunicator->numCommBatches();
}

void HyPerCol::ioParam_errorOnNotANumber(enum ParamsIOFlag ioFlag) {
   parameters()->ioParamValue(
         ioFlag, mName, "errorOnNotANumber", &mErrorOnNotANumber, mErrorOnNotANumber);
}

// Sep 26, 2016: HyPerCol methods for parameter input/output have been moved to
// PVParams.
// Sep 27, 2016: ensureDirExists has been moved to fileio.cpp.

int HyPerCol::addLayer(HyPerLayer *layer) {
   addObject(layer);
   mLayers.push_back(layer);
   if (layer->getPhase() >= mNumPhases)
      mNumPhases = layer->getPhase() + 1;
   return mLayers.size() - 1;
}

int HyPerCol::addConnection(BaseConnection *conn) {
   addObject(conn);
   mConnections.push_back(conn);
   return mConnections.size() - 1;
}

int HyPerCol::addNormalizer(NormalizeBase *normalizer) {
   mNormalizers.push_back(normalizer);
   return PV_SUCCESS; // Why does this return success when the other add
   // functions return an index?
}

void HyPerCol::allocateColumn() {
   if (mReadyFlag) {
      return;
   }

#ifdef PV_USE_CUDA
   // Default to auto assign gpus
   std::string const &gpu_devices = mPVInitObj->getStringArgument("GPUDevices");
   initializeCUDA(gpu_devices);
#endif

   setNumThreads(false);
   // When we call processParams, the communicateInitInfo stage will run, which
   // can put out a lot of messages.
   // So if there's a problem with the -t option setting, the error message can
   // be hard to find.
   // Instead of printing the error messages here, we will call setNumThreads a
   // second time after processParams(), and only then print messages.

   // processParams function does communicateInitInfo stage, sets up adaptive
   // time step, and prints params
   pvAssert(mPrintParamsFilename && mPrintParamsFilename[0]);
   if (mPrintParamsFilename[0] != '/') {
      std::string printParamsFilename(mPrintParamsFilename);
      std::string printParamsPath = mCheckpointer->makeOutputPathFilename(printParamsFilename);
      processParams(printParamsPath.c_str());
   }
   else {
      // If using absolute path, only global rank 0 writes, to avoid collisions.
      if (mCheckpointer->getMPIBlock()->getGlobalRank() == 0) {
         processParams(mPrintParamsFilename);
      }
   }

   int thread_status =
         setNumThreads(true /*now, print messages related to setting number of threads*/);
   MPI_Barrier(mCommunicator->globalCommunicator());
   if (thread_status != PV_SUCCESS) {
      exit(EXIT_FAILURE);
   }

#ifdef PV_USE_OPENMP_THREADS
   pvAssert(mNumThreads > 0); // setNumThreads should fail if it sets
   // mNumThreads less than or equal to zero
   omp_set_num_threads(mNumThreads);
#endif // PV_USE_OPENMP_THREADS

   notify(std::make_shared<AllocateDataMessage>());

   mPhaseRecvTimers.clear();
   for (int phase = 0; phase < mNumPhases; phase++) {
      std::string timerTypeString("phRecv");
      timerTypeString.append(std::to_string(phase));
      Timer *phaseRecvTimer = new Timer(mName, "column", timerTypeString.c_str());
      mPhaseRecvTimers.push_back(phaseRecvTimer);
      mCheckpointer->registerTimer(phaseRecvTimer);
   }

   notify(std::make_shared<RegisterDataMessage<Checkpointer>>(mCheckpointer));

#ifdef DEBUG_OUTPUT
   InfoLog().printf("[%d]: HyPerCol: running...\n", mCommunicator->globalCommRank());
   InfoLog().flush();
#endif

   // Initialize the state of each object based on the params file,
   // and then if reading from checkpoint, call the checkpointer.
   // This needs to happen after initPublishers so that we can initialize
   // the values in the data stores, and before the mLayers' publish calls
   // so that the data in border regions gets copied correctly.
   notify(std::make_shared<InitializeStateMessage>());
   if (mCheckpointReadFlag) {
      mCheckpointer->checkpointRead(&mSimTime, &mCurrentStep);
   }
   else {
      mCheckpointer->readStateFromCheckpoint();
      // readStateFromCheckpoint() does nothing if initializeFromCheckpointDir is empty or null.
   }
// Note: ideally, if checkpointReadFlag is set, calling InitializeState should
// be unnecessary. However, currently initializeState does some CUDA kernel
// initializations that still need to happen when reading from checkpoint.

#ifdef PV_USE_CUDA
   notify(std::make_shared<CopyInitialStateToGPUMessage>());
#endif // PV_USE_CUDA

   // Initial normalization moved here to facilitate normalizations of groups
   // of HyPerConns
   normalizeWeights();
   notify(std::make_shared<ConnectionFinalizeUpdateMessage>(mSimTime, mDeltaTime));

   // publish initial conditions
   for (int phase = 0; phase < mNumPhases; phase++) {
      notify(std::make_shared<LayerPublishMessage>(phase, mSimTime));
   }

   // Feb 2, 2017: waiting and updating active indices have been moved into
   // OutputState and CheckNotANumber, where they are called if needed.

   // output initial conditions
   if (!mCheckpointReadFlag) {
      notify(std::make_shared<ConnectionOutputMessage>(mSimTime));
      for (int phase = 0; phase < mNumPhases; phase++) {
         notify(std::make_shared<LayerOutputStateMessage>(phase, mSimTime));
      }
   }
   mReadyFlag = true;
}

// typically called by buildandrun via HyPerCol::run()
int HyPerCol::run(double start_time, double stop_time, double dt) {
   mStartTime = start_time;
   mStopTime  = stop_time;
   mDeltaTime = dt;

   allocateColumn();

   bool dryRunFlag = mPVInitObj->getBooleanArgument("DryRun");
   if (dryRunFlag) {
      return PV_SUCCESS;
   }

#ifdef TIMER_ON
   Clock runClock;
   runClock.start_clock();
#endif

   advanceTimeLoop(runClock, 10 /*runClockStartingStep*/);

   notify(std::make_shared<CleanupMessage>());

#ifdef DEBUG_OUTPUT
   InfoLog().printf("[%d]: HyPerCol: done...\n", mCommunicator->globalCommRank());
   InfoLog().flush();
#endif

   mCheckpointer->finalCheckpoint(mSimTime);

#ifdef TIMER_ON
   runClock.stop_clock();
   runClock.print_elapsed(getOutputStream());
#endif

   return PV_SUCCESS;
}

// Sep 26, 2016: Adaptive timestep routines and member variables have been moved
// to
// AdaptiveTimeScaleProbe.

// This routine sets the mNumThreads member variable.  It should only be called
// by the run() method,
// and only inside the !ready if-statement.
// TODO: Instead of using the printMessagesFlag, why not use the same flag that
int HyPerCol::setNumThreads(bool printMessagesFlag) {
   bool printMsgs0   = printMessagesFlag && globalRank() == 0;
   int thread_status = PV_SUCCESS;
   int num_threads   = 0;
#ifdef PV_USE_OPENMP_THREADS
   int max_threads = mPVInitObj->getMaxThreads();
   int comm_size   = mCommunicator->globalCommSize();
   if (printMsgs0) {
      InfoLog().printf(
            "Maximum number of OpenMP threads%s is %d\n"
            "Number of MPI processes is %d.\n",
            comm_size == 1 ? "" : " (over all processes)",
            max_threads,
            comm_size);
   }
   Configuration::IntOptional numThreadsArg = mPVInitObj->getIntOptionalArgument("NumThreads");
   if (numThreadsArg.mUseDefault) {
      num_threads = max_threads / comm_size; // integer arithmetic
      if (num_threads == 0) {
         num_threads = 1;
         if (printMsgs0) {
            WarnLog().printf(
                  "Warning: more MPI processes than available threads.  "
                  "Processors may be oversubscribed.\n");
         }
      }
   }
   else {
      num_threads = numThreadsArg.mValue;
   }
   if (num_threads > 0) {
      if (printMsgs0) {
         InfoLog().printf("Number of threads used is %d\n", num_threads);
      }
   }
   else if (num_threads == 0) {
      thread_status = PV_FAILURE;
      if (printMsgs0) {
         ErrorLog().printf(
               "%s: number of threads must be positive (was set to zero)\n",
               mPVInitObj->getProgramName());
      }
   }
   else {
      assert(num_threads < 0);
      thread_status = PV_FAILURE;
      if (printMsgs0) {
         ErrorLog().printf(
               "%s was compiled with PV_USE_OPENMP_THREADS; "
               "therefore the \"-t\" argument is "
               "required.\n",
               mPVInitObj->getProgramName());
      }
   }
#else // PV_USE_OPENMP_THREADS
   Configuration::IntOptional numThreadsArg = mPVInitObj->getIntOptionalArgument("NumThreads");
   if (numThreadsArg.mUseDefault) {
      num_threads = 1;
      if (printMsgs0) {
         InfoLog().printf("Number of threads used is 1 (Compiled without OpenMP.\n");
      }
   }
   else {
      num_threads = numThreadsArg.mValue;
      if (num_threads < 0) {
         num_threads = 1;
      }
      if (num_threads != 1) {
         thread_status = PV_FAILURE;
      }
   }
   if (printMsgs0) {
      if (thread_status != PV_SUCCESS) {
         ErrorLog().printf(
               "%s error: PetaVision must be compiled with "
               "OpenMP to run with threads.\n",
               mPVInitObj->getProgramName());
      }
   }
#endif // PV_USE_OPENMP_THREADS
   mNumThreads = num_threads;
   return thread_status;
}

int HyPerCol::processParams(char const *path) {
   if (!mParamsProcessedFlag) {
      auto const &objectMap = mObjectHierarchy.getObjectMap();
      notify(std::make_shared<CommunicateInitInfoMessage>(objectMap));
   }

   // Print a cleaned up version of params to the file given by
   // printParamsFilename
   parameters()->warnUnread();
   if (path != nullptr && path[0] != '\0') {
      outputParams(path);
   }
   else {
      if (globalRank() == 0) {
         InfoLog().printf(
               "HyPerCol \"%s\": path for printing parameters file was "
               "empty or null.\n",
               mName);
      }
   }
   mParamsProcessedFlag = true;
   return PV_SUCCESS;
}

int HyPerCol::normalizeWeights() {
   int status = PV_SUCCESS;
   for (int n = 0; n < mNormalizers.size(); n++) {
      NormalizeBase *normalizer = mNormalizers.at(n);
      if (normalizer) {
         status = normalizer->normalizeWeightsWrapper();
      }
      if (status != PV_SUCCESS) {
         ErrorLog().printf("Normalizer \"%s\" failed.\n", mNormalizers[n]->getName());
      }
   }
   return status;
}

// Sep 26, 2016: Adaptive timestep routines and member variables have been moved
// to
// AdaptiveTimeScaleProbe.

void HyPerCol::advanceTimeLoop(Clock &runClock, int const runClockStartingStep) {
   // time loop
   //
   long int step = 0;
   while (mSimTime < mStopTime - mDeltaTime / 2.0) {
      mCheckpointer->checkpointWrite(mSimTime);
      advanceTime(mSimTime);

      step += 1;
#ifdef TIMER_ON
      if (step == runClockStartingStep) {
         runClock.start_clock();
      }
#endif

   } // end time loop
}

int HyPerCol::advanceTime(double sim_time) {
   if (mSimTime >= mNextProgressTime) {
      mNextProgressTime += mProgressInterval;
      if (mCommunicator->globalCommRank() == 0) {
         std::ostream &progressStream = mWriteProgressToErr ? getErrorStream() : getOutputStream();
         time_t current_time;
         time(&current_time);
         progressStream << "   time==" << sim_time << "  "
                        << ctime(&current_time); // ctime outputs an newline
      }
   }

   mRunTimer->start();

   // make sure mSimTime is updated even if HyPerCol isn't running time loop
   // triggerOffset might fail if mSimTime does not advance uniformly because
   // mSimTime could skip over tigger event
   // !!!TODO: fix trigger layer to compute mTimeScale so as not to allow
   // bypassing trigger event
   mSimTime = sim_time + mDeltaTime;

   notify(std::make_shared<AdaptTimestepMessage>());
   // Sep 26, 2016: Adaptive timestep routines and member variables have been
   // moved to
   // AdaptiveTimeScaleProbe.

   // At this point all activity from the previous time step has
   // been delivered to the data store.
   //

   int status = PV_SUCCESS;

   // update the connections (weights)
   //
   notify(std::make_shared<ConnectionUpdateMessage>(mSimTime, mDeltaTime));
   normalizeWeights();
   notify(std::make_shared<ConnectionFinalizeUpdateMessage>(mSimTime, mDeltaTime));
   notify(std::make_shared<ConnectionOutputMessage>(mSimTime));

   // Each layer's phase establishes a priority for updating
   for (int phase = 0; phase < mNumPhases; phase++) {
      for (auto &l : mLayers) { // TODO: use notify/respond
         l->clearProgressFlags();
      }

// nonblockingLayerUpdate allows for more concurrency than notify.
#ifdef PV_USE_CUDA
      // Ordering needs to go recvGpu, if(recvGpu and upGpu)update, recvNoGpu,
      // update rest
      auto recvMessage = std::make_shared<LayerRecvSynapticInputMessage>(
            phase, mPhaseRecvTimers.at(phase), true /*recvGpuFlag*/, mSimTime, mDeltaTime);
      auto updateMessage = std::make_shared<LayerUpdateStateMessage>(
            phase, true /*recvGpuFlag*/, true /*updateGpuFlag*/, mSimTime, mDeltaTime);
      nonblockingLayerUpdate(recvMessage, updateMessage);

      recvMessage = std::make_shared<LayerRecvSynapticInputMessage>(
            phase, mPhaseRecvTimers.at(phase), false /*recvGpuFlag*/, mSimTime, mDeltaTime);
      updateMessage = std::make_shared<LayerUpdateStateMessage>(
            phase, false /*recvGpuFlag*/, false /*updateGpuFlag*/, mSimTime, mDeltaTime);
      nonblockingLayerUpdate(recvMessage, updateMessage);

      getDevice()->syncDevice();

      // Update for receiving on cpu and updating on gpu
      notify(
            std::make_shared<LayerUpdateStateMessage>(
                  phase, false /*recvOnGpuFlag*/, true /*updateOnGpuFlag*/, mSimTime, mDeltaTime));

      getDevice()->syncDevice();
      notify(std::make_shared<LayerCopyFromGpuMessage>(phase, mPhaseRecvTimers.at(phase)));

      // Update for gpu recv and non gpu update
      notify(
            std::make_shared<LayerUpdateStateMessage>(
                  phase, true /*recvOnGpuFlag*/, false /*updateOnGpuFlag*/, mSimTime, mDeltaTime));
#else
      auto recvMessage = std::make_shared<LayerRecvSynapticInputMessage>(
            phase, mPhaseRecvTimers.at(phase), mSimTime, mDeltaTime);
      auto updateMessage = std::make_shared<LayerUpdateStateMessage>(phase, mSimTime, mDeltaTime);
      nonblockingLayerUpdate(recvMessage, updateMessage);
#endif
      // Rotate DataStore ring buffers
      notify(std::make_shared<LayerAdvanceDataStoreMessage>(phase));

      // copy activity buffer to DataStore, and do MPI exchange.
      notify(std::make_shared<LayerPublishMessage>(phase, mSimTime));

      // Feb 2, 2017: waiting and updating active indices have been moved into
      // OutputState and CheckNotANumber, where they are called if needed.
      notify(std::make_shared<LayerOutputStateMessage>(phase, mSimTime));
      if (mErrorOnNotANumber) {
         notify(std::make_shared<LayerCheckNotANumberMessage>(phase));
      }
   }

   mRunTimer->stop();

   outputState(mSimTime);

   return status;
}

void HyPerCol::nonblockingLayerUpdate(
      std::shared_ptr<LayerRecvSynapticInputMessage const> recvMessage,
      std::shared_ptr<LayerUpdateStateMessage const> updateMessage) {
   long int idleCounter = 0;
   bool allUpdated      = false;
   int phase            = recvMessage->mPhase;
   pvAssert(phase == updateMessage->mPhase);
   while (!allUpdated) {
      bool anyUpdated = false;

      for (auto &l : mLayers) {
#ifdef PV_USE_CUDA
         if (l->getRecvGpu() != recvMessage->mRecvOnGpuFlag) {
            continue;
         }
#endif // PV_USE_CUDA
         if (l->getPhase() == phase && !l->getHasReceived() && l->isAllInputReady()) {
            l->respond(recvMessage);
            anyUpdated = true;
            break;
         }
      }
      if (anyUpdated) {
         continue;
      }

      for (auto &l : mLayers) {
#ifdef PV_USE_CUDA
         if (l->getRecvGpu() != updateMessage->mRecvOnGpuFlag) {
            continue;
         }
         if (l->getUpdateGpu() != updateMessage->mUpdateOnGpuFlag) {
            continue;
         }
#endif // PV_USE_CUDA
         if (l->getPhase() == phase && !l->getHasUpdated() && l->getHasReceived()) {
            l->respond(updateMessage);
            anyUpdated = true;
            break;
         }
      }
      if (anyUpdated) {
         continue;
      }

      idleCounter++;
      // TODO (maybe?) Waitany logic goes here.
      allUpdated = true;
      for (auto &l : mLayers) {
#ifdef PV_USE_CUDA
         if (l->getRecvGpu() != updateMessage->mRecvOnGpuFlag) {
            continue;
         }
         if (l->getUpdateGpu() != updateMessage->mUpdateOnGpuFlag) {
            continue;
         }
#endif // PV_USE_CUDA
         if (l->getPhase() == phase) {
            allUpdated &= l->getHasUpdated();
         }
      }
   }
   if (idleCounter > 1L) {
      InfoLog() << "t = " << mSimTime << ", phase " << phase << ", idle count " << idleCounter
                << "\n";
   }
}

// Oct 3, 2016.  writeTimers moved to Checkpointer class

int HyPerCol::respond(std::shared_ptr<BaseMessage const> message) {
   int status = PV_SUCCESS;
   if (PrepareCheckpointWriteMessage const *castMessage =
             dynamic_cast<PrepareCheckpointWriteMessage const *>(message.get())) {
      status = respondPrepareCheckpointWrite(castMessage);
   }
   return status;
}

// Oct 20, 2016. checkpointWrite and checkpointRead functionality moved to Checkpointer class.

int HyPerCol::respondPrepareCheckpointWrite(PrepareCheckpointWriteMessage const *message) {
   std::string path(message->mDirectory);
   path.append("/").append("pv.params");
   return outputParams(path.c_str());
}

int HyPerCol::outputParams(char const *path) {
   assert(path != nullptr && path[0] != '\0');
   int status = PV_SUCCESS;
   int rank   = mCheckpointer->getMPIBlock()->getRank();
   assert(mPrintParamsStream == nullptr);
   char *tmp = strdup(path); // duplicate string since dirname() is allowed to
   // modify its argument
   if (tmp == nullptr) {
      Fatal().printf("HyPerCol::outputParams unable to allocate memory: %s\n", strerror(errno));
   }
   char *containingdir = dirname(tmp);
   status              = ensureDirExists(mCheckpointer->getMPIBlock(), containingdir);
   if (status != PV_SUCCESS) {
      ErrorLog().printf(
            "HyPerCol::outputParams unable to create directory \"%s\"\n", containingdir);
   }
   free(tmp);
   if (rank == 0) {
      mPrintParamsStream = new FileStream(path, std::ios_base::out, getVerifyWrites());
      // Get new lua path
      std::string luaPath(path);
      luaPath.append(".lua");
      char luapath[PV_PATH_MAX];
      mLuaPrintParamsStream =
            new FileStream(luaPath.c_str(), std::ios_base::out, getVerifyWrites());
      parameters()->setPrintParamsStream(mPrintParamsStream);
      parameters()->setPrintLuaStream(mLuaPrintParamsStream);

      // Params file output
      outputParamsHeadComments(mPrintParamsStream, "//");

      // Lua file output
      outputParamsHeadComments(mLuaPrintParamsStream, "--");
      // Load util module based on PVPath
      mLuaPrintParamsStream->printf(
            "package.path = package.path .. \";\" .. \"" PV_DIR "/../parameterWrapper/?.lua\"\n");
      mLuaPrintParamsStream->printf("local pv = require \"PVModule\"\n\n");
      mLuaPrintParamsStream->printf("-- Base table variable to store\n");
      mLuaPrintParamsStream->printf("local pvParameters = {\n");
   }

   // Parent HyPerCol params
   status = ioParams(PARAMS_IO_WRITE);
   if (status != PV_SUCCESS) {
      Fatal().printf("outputParams: Error copying params to \"%s\"\n", path);
   }

   // HyPerLayer params
   for (int l = 0; l < mLayers.size(); l++) {
      HyPerLayer *layer = mLayers.at(l);
      status            = layer->ioParams(PARAMS_IO_WRITE);
      if (status != PV_SUCCESS) {
         Fatal().printf("outputParams: Error copying params to \"%s\"\n", path);
      }
   }

   // BaseConnection params
   for (auto c : mConnections) {
      status = c->ioParams(PARAMS_IO_WRITE);
      if (status != PV_SUCCESS) {
         Fatal().printf("outputParams: Error copying params to \"%s\"\n", path);
      }
   }

   // Probe params

   // ColProbes
   for (int p = 0; p < mColProbes.size(); p++) {
      mColProbes.at(p)->ioParams(PARAMS_IO_WRITE);
   }

   // LayerProbes
   for (int l = 0; l < mLayers.size(); l++) {
      mLayers.at(l)->outputProbeParams();
   }

   // BaseConnectionProbes
   for (auto c : mConnections) {
      c->outputProbeParams();
   }

   if (rank == 0) {
      mLuaPrintParamsStream->printf("} --End of pvParameters\n");
      mLuaPrintParamsStream->printf(
            "\n-- Print out PetaVision approved parameter file to the console\n");
      mLuaPrintParamsStream->printf("paramsFileString = pv.createParamsFileString(pvParameters)\n");
      mLuaPrintParamsStream->printf("io.write(paramsFileString)\n");
   }

   if (mPrintParamsStream) {
      delete mPrintParamsStream;
      mPrintParamsStream = nullptr;
      parameters()->setPrintParamsStream(mPrintParamsStream);
   }
   if (mLuaPrintParamsStream) {
      delete mLuaPrintParamsStream;
      mLuaPrintParamsStream = nullptr;
      parameters()->setPrintLuaStream(mLuaPrintParamsStream);
   }
   return status;
}

int HyPerCol::outputParamsHeadComments(FileStream *fileStream, char const *commentToken) {
   time_t t = time(nullptr);
   fileStream->printf("%s PetaVision, " PV_REVISION "\n", commentToken);
   fileStream->printf("%s Run time %s", commentToken, ctime(&t)); // output of ctime contains \n
#ifdef PV_USE_MPI
   MPIBlock const *mpiBlock = mCheckpointer->getMPIBlock();
   fileStream->printf(
         "%s Compiled with MPI and run using %d rows, %d columns, and MPI batch dimension %d.\n",
         commentToken,
         mpiBlock->getGlobalNumRows(),
         mpiBlock->getGlobalNumColumns(),
         mpiBlock->getGlobalBatchDimension());
   if (mpiBlock->getNumRows() < mpiBlock->getGlobalNumRows()
       or mpiBlock->getNumColumns() < mpiBlock->getGlobalNumColumns()
       or mpiBlock->getBatchDimension() < mpiBlock->getGlobalBatchDimension()) {
      fileStream->printf(
            "%s CheckpointCells have %d rows, %d columns, and MPI batch dimension %d.\n",
            commentToken,
            mpiBlock->getNumRows(),
            mpiBlock->getNumColumns(),
            mpiBlock->getBatchDimension());
   }
#else // PV_USE_MPI
   fileStream->printf("%s Compiled without MPI.\n", commentToken);
#endif // PV_USE_MPI
#ifdef PV_USE_CUDA
   fileStream->printf("%s Compiled with CUDA.\n", commentToken);
#else
   fileStream->printf("%s Compiled without CUDA.\n", commentToken);
#endif
#ifdef PV_USE_OPENMP_THREADS
   fileStream->printf("%s Compiled with OpenMP parallel code", commentToken);
   if (mNumThreads > 0) {
      fileStream->printf(" and run using %d threads.\n", mNumThreads);
   }
   else if (mNumThreads == 0) {
      fileStream->printf(" but number of threads was set to zero (error).\n");
   }
   else {
      fileStream->printf(" but the -t option was not specified.\n");
   }
#else
   fileStream->printf("%s Compiled without OpenMP parallel code", commentToken);
   if (mNumThreads == 1) {
      fileStream->printf(".\n");
   }
   else if (mNumThreads == 0) {
      fileStream->printf(" but number of threads was set to zero (error).\n");
   }
   else {
      fileStream->printf(
            " but number of threads specified was %d instead of 1. (error).\n", mNumThreads);
   }
#endif // PV_USE_OPENMP_THREADS
   if (mCheckpointReadFlag) {
      fileStream->printf(
            "%s Started from checkpoint \"%s\"\n",
            commentToken,
            mCheckpointer->getCheckpointReadDirectory().c_str());
   }
   return PV_SUCCESS;
}

// Nov 22, 2016: pathInCheckpoint removed. Made unnecessary by the Checkpointer refactor.

int HyPerCol::getAutoGPUDevice() {
   int returnGpuIdx = -1;
#ifdef PV_USE_CUDA
   int mpiRank = mCommunicator->globalCommRank();
   int numMpi  = mCommunicator->globalCommSize();
   char hostNameStr[PV_PATH_MAX];
   gethostname(hostNameStr, PV_PATH_MAX);
   size_t hostNameLen = strlen(hostNameStr) + 1; //+1 for null terminator

   // Each rank communicates which host it is on
   // Root process
   if (mpiRank == 0) {
      // Allocate data structure for rank to host
      char rankToHost[numMpi][PV_PATH_MAX];
      assert(rankToHost);
      // Allocate data structure for rank to maxGpu
      int rankToMaxGpu[numMpi];
      // Allocate final data structure for rank to GPU index
      int rankToGpu[numMpi];
      assert(rankToGpu);

      for (int rank = 0; rank < numMpi; rank++) {
         if (rank == 0) {
            strcpy(rankToHost[rank], hostNameStr);
            rankToMaxGpu[rank] = PVCuda::CudaDevice::getNumDevices();
         }
         else {
            MPI_Recv(
                  rankToHost[rank],
                  PV_PATH_MAX,
                  MPI_CHAR,
                  rank,
                  0,
                  mCommunicator->globalCommunicator(),
                  MPI_STATUS_IGNORE);
            MPI_Recv(
                  &(rankToMaxGpu[rank]),
                  1,
                  MPI_INT,
                  rank,
                  0,
                  mCommunicator->globalCommunicator(),
                  MPI_STATUS_IGNORE);
         }
      }

      // rankToHost now is an array such that the index is the rank, and the value
      // is the host
      // Convert to a map of vectors, such that the key is the host name and the
      // value
      // is a vector of mpi ranks that is running on that host
      std::map<std::string, std::vector<int>> hostMap;
      for (int rank = 0; rank < numMpi; rank++) {
         hostMap[std::string(rankToHost[rank])].push_back(rank);
      }

      // Determine what gpus to use per mpi
      for (auto &host : hostMap) {
         std::vector<int> rankVec = host.second;
         int numRanksPerHost      = rankVec.size();
         assert(numRanksPerHost > 0);
         // Grab maxGpus of current host
         int maxGpus = rankToMaxGpu[rankVec[0]];
         // Warnings for overloading/underloading gpus
         if (numRanksPerHost != maxGpus) {
            WarnLog(assignGpuWarning);
            assignGpuWarning.printf(
                  "HyPerCol::getAutoGPUDevice: Host \"%s\" (rank[s] ", host.first.c_str());
            for (int v_i = 0; v_i < numRanksPerHost; v_i++) {
               if (v_i != numRanksPerHost - 1) {
                  assignGpuWarning.printf("%d, ", rankVec[v_i]);
               }
               else {
                  assignGpuWarning.printf("%d", rankVec[v_i]);
               }
            }
            assignGpuWarning.printf(
                  ") is being %s, with %d mpi processes mapped to %d total GPU[s]\n",
                  numRanksPerHost < maxGpus ? "underloaded" : "overloaded",
                  numRanksPerHost,
                  maxGpus);
         }

         // Match a rank to a gpu
         for (int v_i = 0; v_i < numRanksPerHost; v_i++) {
            rankToGpu[rankVec[v_i]] = v_i % maxGpus;
         }
      }

      // MPI sends to each process to specify which gpu the rank should use
      for (int rank = 0; rank < numMpi; rank++) {
         InfoLog() << "Rank " << rank << " on host \"" << rankToHost[rank] << "\" ("
                   << rankToMaxGpu[rank] << " GPU[s]) using GPU index " << rankToGpu[rank] << "\n";
         if (rank == 0) {
            returnGpuIdx = rankToGpu[rank];
         }
         else {
            MPI_Send(&(rankToGpu[rank]), 1, MPI_INT, rank, 0, mCommunicator->globalCommunicator());
         }
      }
   }
   // Non root process
   else {
      // Send host name
      MPI_Send(hostNameStr, hostNameLen, MPI_CHAR, 0, 0, mCommunicator->globalCommunicator());
      // Send max gpus for that host
      int maxGpu = PVCuda::CudaDevice::getNumDevices();
      MPI_Send(&maxGpu, 1, MPI_INT, 0, 0, mCommunicator->globalCommunicator());
      // Recv gpu idx
      MPI_Recv(
            &(returnGpuIdx),
            1,
            MPI_INT,
            0,
            0,
            mCommunicator->globalCommunicator(),
            MPI_STATUS_IGNORE);
   }
   assert(returnGpuIdx >= 0 && returnGpuIdx < PVCuda::CudaDevice::getNumDevices());
#else
   // This function should never be called when not running with GPUs
   assert(false);
#endif
   return returnGpuIdx;
}

#ifdef PV_USE_CUDA
void HyPerCol::initializeCUDA(std::string const &in_device) {
   // Don't do anything unless some object needs CUDA.
   bool needGPU    = false;
   auto &objectMap = mObjectHierarchy.getObjectMap();
   for (auto &obj : objectMap) {
      Observer *observer = obj.second;
      BaseObject *object = dynamic_cast<BaseObject *>(observer);
      pvAssert(object); // Only addObject(BaseObject*) can change the hierarchy.
      if (object->isUsingGPU()) {
         needGPU = true;
         break;
      }
   }
   if (!needGPU) {
      return;
   }

   int numMpi = mCommunicator->globalCommSize();
   int device;

   // default value
   if (in_device.empty()) {
      InfoLog() << "Auto assigning GPUs\n";
      device = getAutoGPUDevice();
   }
   else {
      std::vector<int> deviceVec;
      std::stringstream ss(in_device);
      std::string stoken;
      // Grabs strings from ss into item, separated by commas
      while (std::getline(ss, stoken, ',')) {
         // Convert stoken to integer
         for (auto &ch : stoken) {
            if (!isdigit(ch)) {
               Fatal().printf(
                     "Device specification error: %s contains "
                     "unrecognized characters. Must be "
                     "comma separated integers greater or equal to 0 "
                     "with no other characters "
                     "allowed (including spaces).\n",
                     in_device.c_str());
            }
         }
         deviceVec.push_back(atoi(stoken.c_str()));
      }
      // Check length of deviceVec
      // Allowed cases are 1 device specified or greater than or equal to number
      // of mpi processes
      // devices specified
      if (deviceVec.size() == 1) {
         device = deviceVec[0];
      }
      else if (deviceVec.size() >= numMpi) {
         device = deviceVec[mCommunicator->globalCommRank()];
      }
      else {
         Fatal().printf(
               "Device specification error: Number of devices "
               "specified (%zu) must be either 1 or "
               ">= than number of mpi processes (%d).\n",
               deviceVec.size(),
               numMpi);
      }
      InfoLog() << "Global MPI Process " << mCommunicator->globalCommRank() << " using device "
                << device << "\n";
   }

   mCudaDevice = new PVCuda::CudaDevice(device);

   // Only print rank for comm rank 0
   if (globalRank() == 0) {
      mCudaDevice->query_device_info();
   }
}

int HyPerCol::finalizeCUDA() {
   delete mCudaDevice;
   for (auto iterator = mGpuGroupConns.begin(); iterator != mGpuGroupConns.end();) {
      delete *iterator;
      iterator = mGpuGroupConns.erase(iterator);
   }
   return 0;
}

void HyPerCol::addGpuGroup(BaseConnection *conn, int gpuGroupIdx) {
   // default gpuGroupIdx is -1, so do nothing if this is the case
   if (gpuGroupIdx < 0) {
      return;
   }
   mGpuGroupConns.reserve(gpuGroupIdx);
   if (mGpuGroupConns.at(gpuGroupIdx) == nullptr) {
      mGpuGroupConns.at(gpuGroupIdx) = conn;
   }
}
#endif // PV_USE_CUDA

int HyPerCol::insertProbe(ColProbe *p) {
   mColProbes.push_back(p);
   return mColProbes.size(); // Other insert functions return the index of the
   // inserted object. Is
   // this correct here?
}

void HyPerCol::addObject(BaseObject *obj) {
   bool succeeded = mObjectHierarchy.addObject(obj->getName(), obj);
   FatalIf(!succeeded, "Adding %s failed.\n", getDescription_c());
}

// BaseProbes include layer probes, connection probes, and column probes.
int HyPerCol::addBaseProbe(BaseProbe *p) {
   addObject(p);
   mBaseProbes.push_back(p);
   return mBaseProbes.size();
}

int HyPerCol::outputState(double time) {
   for (int n = 0; n < mColProbes.size(); n++) {
      mColProbes.at(n)->outputStateWrapper(time, mDeltaTime);
   }
   return PV_SUCCESS;
}

HyPerLayer *HyPerCol::getLayerFromName(const char *layerName) {
   if (layerName == nullptr) {
      return nullptr;
   }
   int n = numberOfLayers();
   for (int i = 0; i < n; i++) {
      HyPerLayer *curLayer = getLayer(i);
      assert(curLayer);
      const char *curLayerName = curLayer->getName();
      assert(curLayerName);
      if (!strcmp(curLayer->getName(), layerName))
         return curLayer;
   }
   return nullptr;
}

BaseConnection *HyPerCol::getConnFromName(const char *connName) {
   if (connName == nullptr)
      return nullptr;
   int n = numberOfConnections();
   for (int i = 0; i < n; i++) {
      BaseConnection *curConn = getConnection(i);
      assert(curConn);
      const char *curConnName = curConn->getName();
      assert(curConnName);
      if (!strcmp(curConn->getName(), connName))
         return curConn;
   }
   return nullptr;
}

NormalizeBase *HyPerCol::getNormalizerFromName(const char *normalizerName) {
   if (normalizerName == nullptr)
      return nullptr;
   int n = numberOfNormalizers();
   for (int i = 0; i < n; i++) {
      NormalizeBase *curNormalizer = getNormalizer(i);
      assert(curNormalizer);
      const char *curNormalizerName = curNormalizer->getName();
      assert(curNormalizerName);
      if (!strcmp(curNormalizer->getName(), normalizerName))
         return curNormalizer;
   }
   return nullptr;
}

ColProbe *HyPerCol::getColProbeFromName(const char *probeName) {
   if (probeName == nullptr)
      return nullptr;
   ColProbe *p = nullptr;
   int n       = numberOfProbes();
   for (int i = 0; i < n; i++) {
      ColProbe *curColProbe = getColProbe(i);
      const char *curName   = curColProbe->getName();
      assert(curName);
      if (!strcmp(curName, probeName)) {
         p = curColProbe;
         break;
      }
   }
   return p;
}

BaseProbe *HyPerCol::getBaseProbeFromName(const char *probeName) {
   if (probeName == nullptr) {
      return nullptr;
   }
   BaseProbe *p = nullptr;
   int n        = numberOfBaseProbes();
   for (int i = 0; i < n; i++) {
      BaseProbe *curBaseProbe = getBaseProbe(i);
      const char *curName     = curBaseProbe->getName();
      assert(curName);
      if (!strcmp(curName, probeName)) {
         p = curBaseProbe;
         break;
      }
   }
   return p;
}

unsigned int HyPerCol::seedRandomFromWallClock() {
   unsigned long t = 0UL;
   int rootproc    = 0;
   if (mCommunicator->globalCommRank() == rootproc) {
      t = time((time_t *)nullptr);
   }
   MPI_Bcast(&t, 1, MPI_UNSIGNED, rootproc, mCommunicator->globalCommunicator());
   return t;
}

HyPerCol *createHyPerCol(PV_Init *pv_initObj) {
   PVParams *params = pv_initObj->getParams();
   if (params == nullptr) {
      ErrorLog() << "createHyPerCol called without having set params.\n";
      return nullptr;
   }
   int numGroups = params->numberOfGroups();
   if (numGroups == 0) {
      std::string paramsFile = pv_initObj->getStringArgument("ParamsFile");
      ErrorLog() << "Params \"" << paramsFile << "\" does not define any groups.\n";
      return nullptr;
   }
   if (strcmp(params->groupKeywordFromIndex(0), "HyPerCol")) {
      std::string paramsFile = pv_initObj->getStringArgument("ParamsFile");
      ErrorLog() << "First group in the params \"" << paramsFile
                 << "\" does not define a HyPerCol.\n";
      return nullptr;
   }
   char const *colName = params->groupNameFromIndex(0);

   HyPerCol *hc = new HyPerCol(colName, pv_initObj);
   for (int k = 0; k < numGroups; k++) {
      const char *kw   = params->groupKeywordFromIndex(k);
      const char *name = params->groupNameFromIndex(k);
      if (!strcmp(kw, "HyPerCol")) {
         if (k == 0) {
            continue;
         }
         else {
            if (hc->columnId() == 0) {
               std::string paramsFile = pv_initObj->getStringArgument("ParamsFile");
               ErrorLog() << "Group " << k + 1 << " in params file (\"" << paramsFile
                          << "\") is a HyPerCol; only the first group can be a HyPercol.\n";
            }
            delete hc;
            return nullptr;
         }
      }
      else {
         BaseObject *addedObject = Factory::instance()->createByKeyword(kw, name, hc);
         if (addedObject == nullptr) {
            if (hc->globalRank() == 0) {
               ErrorLog().printf("Unable to create %s \"%s\".\n", kw, name);
            }
            delete hc;
            return nullptr;
         }
      }
   }

   return hc;
}

} // PV namespace
