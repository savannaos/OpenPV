/*
 * MatchingPursuitLayer.cpp
 *
 *  Created on: Jul 31, 2013
 *      Author: pschultz
 *
 *  Each call to updateState performs one step of the ordinary matching pursuit algorithm
 *  More specifically, it selects the neuron n such that abs(GSyn(n)) is largest, and
 *  changes the activity of neuron n by the amount GSyn(n).
 *
 *  GSyn is the excitatory GSyn if there is only one channel, and excitatory minus inhibitory
 *  if there is more than one.
 *
 *  Matching pursuit can can therefore be implemented by constructing a column as follows:
 *
 *  [input layer] ----> [residual layer] <----> [matching pursuit layer]
 *  The connection from input layer to residual layer is an IdentConn
 *  The connection from the matching pursuit layer to the residual layer is the dictionary,
 *  and there is also a FeedbackConn from the residual layer to the matching pursuit layer
 *
 *  The reconstruction of the input can be obtained either by making a TransposeConn of
 *  the dictionary from the matching pursuit layer to a separate reconstruction error,
 *  or by subtracting the residual layer from the input layer.
 */

#include "MatchingPursuitLayer.hpp"

namespace PV {

MatchingPursuitLayer::MatchingPursuitLayer() {
   initialize_base();
}

MatchingPursuitLayer::MatchingPursuitLayer(const char * name, HyPerCol * hc) {
   initialize_base();
   initialize(name, hc);
}

MatchingPursuitLayer::~MatchingPursuitLayer() {
   free(syncedMovieName);
   free(traceFileName);
   if (parent->columnId()==0) PV_fclose(traceFile);
}

int MatchingPursuitLayer::initialize_base() {
   numChannels = 2;
   activationThreshold = 0.0f;
   syncedMovieName = NULL;
   syncedMovie = NULL;
   tracePursuit = false;
   traceFile = NULL;
   traceFileName = NULL;
   initializeMaxinfo();
   useWindowedSynapticInput = true;
   xWindowSize = 0;
   yWindowSize = 0;
   return PV_SUCCESS;
}

int MatchingPursuitLayer::initialize(const char * name, HyPerCol * hc) {
   int status = HyPerLayer::initialize(name, hc);

   if (status == PV_SUCCESS) status = openPursuitFile();

   return status;
}

int MatchingPursuitLayer::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = HyPerLayer::ioParamsFillGroup(ioFlag);
   ioParam_activationThreshold(ioFlag);
   ioParam_syncedMovie(ioFlag);
   ioParam_tracePursuit(ioFlag);
   ioParam_traceFile(ioFlag);
   return status;
}

void MatchingPursuitLayer::ioParam_activationThreshold(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "activationThreshold", &activationThreshold, activationThreshold);
}

void MatchingPursuitLayer::ioParam_syncedMovie(enum ParamsIOFlag ioFlag) {
   parent->ioParamString(ioFlag, name, "syncedMovie", &syncedMovieName, NULL);
   if (syncedMovieName && syncedMovieName[0]=='\0') {
      free(syncedMovieName);
      syncedMovieName = NULL;
   }
}

void MatchingPursuitLayer::ioParam_tracePursuit(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "tracePursuit", &tracePursuit, tracePursuit);
}

void MatchingPursuitLayer::ioParam_traceFile(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "tracePursuit"));
   if (tracePursuit) {
      parent->ioParamString(ioFlag, name, "traceFile", &traceFileName, NULL);
   }
}

int MatchingPursuitLayer::openPursuitFile() {
   if (parent->columnId()!=0) return PV_SUCCESS;
   if (traceFileName && traceFileName[0]) {
      size_t tracePathLen = strlen(traceFileName);
      if (traceFileName[0] != '/') {
         tracePathLen += strlen(parent->getOutputPath()) + (size_t) 1; // traceFileName will be <outputPath>/<pursuit_file_name>
      }
      char * tracePath = (char *) malloc(tracePathLen + (size_t) 1); // tracePath plus string terminator
      if(tracePath == NULL) {
         fprintf(stderr, "%s \"%s\" error: Unable to allocate memory for traceFile path: %s\n",
               parent->parameters()->groupKeywordFromName(name), name, strerror(errno));
         exit(EXIT_FAILURE);
      }
      if (traceFileName[0] != '/') {
         int charswritten = snprintf(tracePath, tracePathLen+(size_t) 1, "%s/%s", parent->getOutputPath(), traceFileName);
         assert(charswritten==tracePathLen);
      }
      else {
         memcpy(tracePath, traceFileName, tracePathLen);
         tracePath[tracePathLen] = '\0';
      }
      traceFile = PV_fopen(tracePath, "w");
      free(tracePath); tracePath = NULL;
   }
   else {
      traceFile = PV_stdout();
   }
   return PV_SUCCESS;


   if (traceFileName && traceFileName[0] && traceFileName[0] != '/') {
      int traceFileNameLen = strlen(parent->getOutputPath())+1+strlen(traceFileName)+1; // traceFileName will be <outputPath>/<pursuit_file_name><terminating NUL>
      if (traceFileNameLen >= PV_PATH_MAX) {
         if (parent->columnId()==0) {
            fprintf(stderr, "%s \"%s\": path for tracePursuit file too long.\n",
                  parent->parameters()->groupKeywordFromName(name), name);
         }
#if PV_USE_MPI
         MPI_Barrier(parent->icCommunicator()->communicator());
#endif
         exit(EXIT_FAILURE);
      }
      char * new_traceFileName = (char *) malloc(traceFileNameLen*sizeof(char));
      if (new_traceFileName==NULL) {
         fprintf(stderr, "%s \"%s\" error: rank %d process unable to copy traceFile param: %s\n",
               parent->parameters()->groupKeywordFromName(name), name, parent->columnId(), strerror(errno));
         exit(EXIT_FAILURE);
      }
      int numchars = snprintf(new_traceFileName, traceFileNameLen+1, "%s/%s",parent->getOutputPath(), traceFileName);
      assert(numchars<=traceFileNameLen);
      free(traceFileName);
      traceFileName = new_traceFileName;
   }




   if (traceFileName!=NULL && parent->columnId()==0) {
      assert(traceFileName[0] != '\0');
      traceFile = PV_fopen(traceFileName,"w");
      if (traceFile==NULL) abort();
   }
   else {
      traceFile = PV_stdout();
   }
   return PV_SUCCESS;
}

void MatchingPursuitLayer::initializeMaxinfo(int rank) {
   maxinfo.maxval = 0.0f;
   maxinfo.maxloc = -1;
   maxinfo.mpirank = rank;
}

int MatchingPursuitLayer::communicateInitInfo() {
   int status = HyPerLayer::communicateInitInfo();

   if (syncedMovieName != NULL) {
      assert(syncedMovieName[0]);
      HyPerLayer * syncedLayer = parent->getLayerFromName(syncedMovieName);
      if (syncedLayer==NULL) {
         fprintf(stderr, "MatchingPursuitLayer \"%s\" error: movieLayerName \"%s\" is not a layer in the HyPerCol.\n",
               name, syncedMovieName);
         return(EXIT_FAILURE);
      }
      syncedMovie = dynamic_cast<Movie *>(syncedLayer);
      if (syncedMovie==NULL) {
         fprintf(stderr, "MatchingPursuitLayer \"%s\" error: movieLayerName \"%s\" is not a Movie or Movie-derived layer in the HyPerCol.\n",
               name, syncedMovieName);
         return(EXIT_FAILURE);
      }
   }

   return status;
}

int MatchingPursuitLayer::allocateDataStructures() {
   int status = HyPerLayer::allocateDataStructures();

   for (int c=0; c<parent->numberOfConnections(); c++) {
      HyPerConn * conn = parent->getConnection(c);
      if (strcmp(conn->postSynapticLayerName(), getName())) continue;
      if (conn->getUpdateGSynFromPostPerspective()) {
         int nxp = conn->xPatchSize();
         if (nxp > xWindowSize) xWindowSize = nxp;
         int nyp = conn->yPatchSize();
         if (nyp > yWindowSize) yWindowSize = nyp;
      }
      else {
         useWindowedSynapticInput = false;
         break;
      }
   }

   return status;
}

int MatchingPursuitLayer::resetGSynBuffers(double timed, double dt) {
   // Only reset the GSyn of those neurons in the window.
   for (int k=0; k<getNumNeurons(); k++) {
      if (inWindowRes(0, k)) {
         GSyn[0][k] = 0.0f;
      }
   }
   if (numChannels > 1) {
      for (int k=0; k<getNumNeurons(); k++) {
         if (inWindowRes(0, k)) {
            GSyn[1][k] = 0.0f;
         }
      }
   }
   return PV_SUCCESS;
}

bool MatchingPursuitLayer::inWindowExt(int windowId, int neuronIdxExt) {
   bool inWindow = true;
   if (useWindowedSynapticInput && maxinfo.maxloc>=0) {
      const PVLayerLoc * loc = getLayerLoc();
      // maxinfo.maxloc is global restricted; neuronIdxExt is local extended.
      int neuronIdxRes = kIndexRestricted(neuronIdxExt, loc->nx, loc->ny, loc->nf, loc->nb);
      if (neuronIdxRes >= 0) {
         inWindow = inWindowGlobalRes(neuronIdxRes, loc);
      }
   }
   return inWindow;
}

bool MatchingPursuitLayer::inWindowRes(int windowId, int neuronIdxRes) {
   bool inWindow = true;
   if (useWindowedSynapticInput && maxinfo.maxloc>=0) {
      const PVLayerLoc * loc = getLayerLoc();
      // maxinfo.maxloc is global restricted; neuronIdxExt is local restricted.
      inWindow = inWindowGlobalRes(neuronIdxRes, loc);
   }
   return inWindow;
}

bool MatchingPursuitLayer::inWindowGlobalRes(int neuronIdxRes, const PVLayerLoc * loc) {
   int neuronIdxGlobal = globalIndexFromLocal(neuronIdxRes, *loc);
   int xdiff = kxPos(neuronIdxGlobal, loc->nxGlobal, loc->nyGlobal, loc->nf) - kxPos(maxinfo.maxloc, loc->nxGlobal, loc->nyGlobal, loc->nf);
   int ydiff = kyPos(neuronIdxGlobal, loc->nxGlobal, loc->nyGlobal, loc->nf) - kyPos(maxinfo.maxloc, loc->nxGlobal, loc->nyGlobal, loc->nf);
   return xdiff > -xWindowSize && xdiff < xWindowSize && ydiff > -yWindowSize && ydiff < yWindowSize;

}

int MatchingPursuitLayer::updateState(double timed, double dt) {
   update_timer->start();
   PVLayerLoc loc = *getLayerLoc();
   if (syncedMovie && syncedMovie->getLastUpdateTime() > lastUpdateTime) {
      memset(getV(),0,(size_t) getNumNeurons()*sizeof(pvdata_t));
      lastUpdateTime = parent->simulationTime();
   }
   if (getCLayer()->numActive) {
      int kLocal = localIndexFromGlobal(getCLayer()->activeIndices[0], loc);
      int kExt = kIndexExtended(kLocal, loc.nx, loc.ny, loc.nf, loc.nb);
      getActivity()[kExt] = 0.0f;
      getCLayer()->numActive = 0;
   }

   initializeMaxinfo(parent->columnId()==0);
   maxinfo.mpirank = parent->columnId();
   if (numChannels==1) {
      for (int k=0; k<getNumNeurons(); k++) {
         updateMaxinfo(GSyn[0][k], k);
      }
   }
   else if (numChannels>=1) {
      for (int k=0; k<getNumNeurons(); k++) {
         updateMaxinfo(GSyn[0][k]-GSyn[1][k], k);
      }
   }
   else {
      assert(0);
   }
   maxinfo.maxloc = globalIndexFromLocal(maxinfo.maxloc, *getLayerLoc());

#ifdef PV_USE_MPI
   int rank = parent->columnId();
   int rootproc = 0;
   if (parent->columnId()==0) {
      struct matchingpursuit_mpi_data maxinfobyprocess;
      for (int r=0; r<parent->icCommunicator()->commSize(); r++) {
         if (r==rootproc) {
            maxinfobyprocess.maxloc = maxinfo.maxloc;
            maxinfobyprocess.maxval = maxinfo.maxval;
         }
         else {
            MPI_Recv(&maxinfobyprocess, (int) sizeof(struct matchingpursuit_mpi_data), MPI_BYTE, r, 2091+r, parent->icCommunicator()->communicator(), MPI_STATUS_IGNORE);
         }
         if (fabsf(maxinfobyprocess.maxval) > fabsf(maxinfo.maxval)) {
            maxinfo.maxval = maxinfobyprocess.maxval;
            maxinfo.maxloc = maxinfobyprocess.maxloc;
            maxinfo.mpirank = r;
         }
      }
   }
   else {
      MPI_Send(&maxinfo, (int) sizeof(struct matchingpursuit_mpi_data), MPI_BYTE, rootproc, 2091+rank, parent->icCommunicator()->communicator());
   }
   MPI_Bcast(&maxinfo, (int) sizeof(struct matchingpursuit_mpi_data), MPI_BYTE, rootproc, parent->icCommunicator()->communicator());

   if (maxinfo.mpirank == rank && fabsf(maxinfo.maxval) > activationThreshold) {
      const PVLayerLoc * loc = getLayerLoc();
      int kLocal = localIndexFromGlobal(maxinfo.maxloc, *loc);
      int kExt = kIndexExtended(kLocal, loc->nx, loc->ny, loc->nf, loc->nb);
      getV()[kLocal] += maxinfo.maxval;
      getActivity()[kExt] = maxinfo.maxval;
      getCLayer()->activeIndices[getCLayer()->numActive] = maxinfo.maxloc;
      getCLayer()->numActive++;
      assert(getCLayer()->numActive==1);
   }
#endif // PV_USE_MPI

   update_timer->stop();

   return PV_SUCCESS;
}

void MatchingPursuitLayer::updateMaxinfo(pvdata_t gsyn, int k) {
   bool newmax = fabsf(maxinfo.maxval) < fabsf(gsyn);
   maxinfo.maxloc = newmax ? k : maxinfo.maxloc;
   maxinfo.maxval = newmax ? gsyn : maxinfo.maxval;
}

int MatchingPursuitLayer::outputState(double timed, bool last) {
   // HyPerLayer::outputState already has an io timer so don't duplicate
   int status = HyPerLayer::outputState(timed, last);

   io_timer->start();

   if (parent->columnId()==0 && tracePursuit) {
      const PVLayerLoc * loc = getLayerLoc();
      int x = kxPos(maxinfo.maxloc, loc->nxGlobal, loc->nyGlobal, loc->nf);
      int y = kyPos(maxinfo.maxloc, loc->nxGlobal, loc->nyGlobal, loc->nf);
      int f = featureIndex(maxinfo.maxloc, loc->nxGlobal, loc->nyGlobal, loc->nf);
      if (fabsf(maxinfo.maxval)>activationThreshold) {
         fprintf(traceFile->fp, "Time %f: Neuron %d (x=%d, y=%d, f=%d), activity %f\n", timed, maxinfo.maxloc, x, y, f, maxinfo.maxval);
      }
      else {
         fprintf(traceFile->fp, "Time %f:     No neurons exceeded the activation threshold.\n", timed);
      }
      fflush(traceFile->fp);
   }

   io_timer->stop();

   return status;
}

} /* namespace PV */
