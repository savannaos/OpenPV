/*
 * NormalizeContrastZeroMean.cpp
 *
 *  Created on: Apr 8, 2013
 *      Author: pschultz
 */

#include "NormalizeContrastZeroMean.hpp"

namespace PV {

NormalizeContrastZeroMean::NormalizeContrastZeroMean() { initialize_base(); }

NormalizeContrastZeroMean::NormalizeContrastZeroMean(const char *name, HyPerCol *hc) {
   initialize_base();
   initialize(name, hc);
}

int NormalizeContrastZeroMean::initialize_base() { return PV_SUCCESS; }

int NormalizeContrastZeroMean::initialize(const char *name, HyPerCol *hc) {
   return NormalizeBase::initialize(name, hc);
}

int NormalizeContrastZeroMean::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = NormalizeBase::ioParamsFillGroup(ioFlag);
   ioParam_minSumTolerated(ioFlag);
   return status;
}

void NormalizeContrastZeroMean::ioParam_minSumTolerated(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(
         ioFlag, name, "minSumTolerated", &minSumTolerated, 0.0f, true /*warnIfAbsent*/);
}

void NormalizeContrastZeroMean::ioParam_normalizeFromPostPerspective(enum ParamsIOFlag ioFlag) {
   if (ioFlag == PARAMS_IO_READ) {
      if (parent->parameters()->present(name, "normalizeFromPostPerspective")) {
         if (parent->columnId() == 0) {
            WarnLog().printf(
                  "%s \"%s\": normalizeMethod \"normalizeContrastZeroMean\" doesn't use "
                  "normalizeFromPostPerspective parameter.\n",
                  parent->parameters()->groupKeywordFromName(name),
                  name);
         }
         parent->parameters()->value(
               name, "normalizeFromPostPerspective"); // marks param as having been read
      }
   }
}

int NormalizeContrastZeroMean::normalizeWeights() {
   int status = PV_SUCCESS;

   assert(numConnections >= 1);

   // TODO: need to ensure that all connections in connectionList have same
   // nxp,nyp,nfp,numArbors,numDataPatches
   HyPerConn *conn0 = connectionList[0];
   for (int c = 1; c < numConnections; c++) {
      HyPerConn *conn = connectionList[c];
      if (conn->numberOfAxonalArborLists() != conn0->numberOfAxonalArborLists()) {
         if (parent->columnId() == 0) {
            ErrorLog().printf(
                  "%s: All connections in the normalization group must have the same number of "
                  "arbors (%s has %d; %s has %d).\n",
                  getDescription_c(),
                  conn0->getDescription_c(),
                  conn0->numberOfAxonalArborLists(),
                  conn->getDescription_c(),
                  conn->numberOfAxonalArborLists());
         }
         status = PV_FAILURE;
      }
      if (conn->getNumDataPatches() != conn0->getNumDataPatches()) {
         if (parent->columnId() == 0) {
            ErrorLog().printf(
                  "%s: All connections in the normalization group must have the same number of "
                  "data patches (%s has %d; %s has %d).\n",
                  getDescription_c(),
                  conn0->getDescription_c(),
                  conn0->getNumDataPatches(),
                  conn->getDescription_c(),
                  conn->getNumDataPatches());
         }
         status = PV_FAILURE;
      }
      if (status == PV_FAILURE) {
         MPI_Barrier(parent->getCommunicator()->communicator());
         exit(EXIT_FAILURE);
      }
   }

   float scale_factor = strength;

   status = NormalizeBase::normalizeWeights(); // applies normalize_cutoff threshold and
   // symmetrizeWeights

   int nArbors        = conn0->numberOfAxonalArborLists();
   int numDataPatches = conn0->getNumDataPatches();
   if (normalizeArborsIndividually) {
      for (int arborID = 0; arborID < nArbors; arborID++) {
         for (int patchindex = 0; patchindex < numDataPatches; patchindex++) {
            float sum             = 0.0f;
            float sumsq           = 0.0f;
            int weights_per_patch = 0;
            for (int c = 0; c < numConnections; c++) {
               HyPerConn *conn = connectionList[c];
               int nxp         = conn0->xPatchSize();
               int nyp         = conn0->yPatchSize();
               int nfp         = conn0->fPatchSize();
               weights_per_patch += nxp * nyp * nfp;
               float *dataStartPatch =
                     conn->get_wDataStart(arborID) + patchindex * weights_per_patch;
               accumulateSumAndSumSquared(dataStartPatch, weights_per_patch, &sum, &sumsq);
            }
            if (fabsf(sum) <= minSumTolerated) {
               WarnLog().printf(
                     "for NormalizeContrastZeroMean \"%s\": sum of weights in patch %d of arbor %d "
                     "is within minSumTolerated=%f of zero. Weights in this patch unchanged.\n",
                     this->getName(),
                     patchindex,
                     arborID,
                     (double)minSumTolerated);
               continue;
            }
            float mean = sum / weights_per_patch;
            float var  = sumsq / weights_per_patch - mean * mean;
            for (int c = 0; c < numConnections; c++) {
               HyPerConn *conn = connectionList[c];
               float *dataStartPatch =
                     conn->get_wDataStart(arborID) + patchindex * weights_per_patch;
               subtractOffsetAndNormalize(
                     dataStartPatch,
                     weights_per_patch,
                     sum / weights_per_patch,
                     sqrtf(var) / scale_factor);
            }
         }
      }
   }
   else {
      for (int patchindex = 0; patchindex < numDataPatches; patchindex++) {
         float sum             = 0.0f;
         float sumsq           = 0.0f;
         int weights_per_patch = 0;
         for (int arborID = 0; arborID < nArbors; arborID++) {
            for (int c = 0; c < numConnections; c++) {
               HyPerConn *conn = connectionList[c];
               int nxp         = conn0->xPatchSize();
               int nyp         = conn0->yPatchSize();
               int nfp         = conn0->fPatchSize();
               weights_per_patch += nxp * nyp * nfp;
               float *dataStartPatch =
                     conn->get_wDataStart(arborID) + patchindex * weights_per_patch;
               accumulateSumAndSumSquared(dataStartPatch, weights_per_patch, &sum, &sumsq);
            }
         }
         if (fabsf(sum) <= minSumTolerated) {
            WarnLog().printf(
                  "for NormalizeContrastZeroMean \"%s\": sum of weights in patch %d is within "
                  "minSumTolerated=%f of zero. Weights in this patch unchanged.\n",
                  getName(),
                  patchindex,
                  (double)minSumTolerated);
            continue;
         }
         int count  = weights_per_patch * nArbors;
         float mean = sum / count;
         float var  = sumsq / count - mean * mean;
         for (int arborID = 0; arborID < nArbors; arborID++) {
            for (int c = 0; c < numConnections; c++) {
               HyPerConn *conn = connectionList[c];
               float *dataStartPatch =
                     conn->get_wDataStart(arborID) + patchindex * weights_per_patch;
               subtractOffsetAndNormalize(
                     dataStartPatch, weights_per_patch, mean, sqrtf(var) / scale_factor);
            }
         }
      }
   }

   return status;
}

void NormalizeContrastZeroMean::subtractOffsetAndNormalize(
      float *dataStartPatch,
      int weights_per_patch,
      float offset,
      float normalizer) {
   for (int k = 0; k < weights_per_patch; k++) {
      dataStartPatch[k] -= offset;
      dataStartPatch[k] /= normalizer;
   }
}

int NormalizeContrastZeroMean::accumulateSumAndSumSquared(
      float *dataPatchStart,
      int weights_in_patch,
      float *sum,
      float *sumsq) {
   // Do not call with sum uninitialized.
   // sum, sumsq, max are not cleared inside this routine so that you can accumulate the stats over
   // several patches with multiple calls
   for (int k = 0; k < weights_in_patch; k++) {
      float w = dataPatchStart[k];
      *sum += w;
      *sumsq += w * w;
   }
   return PV_SUCCESS;
}

NormalizeContrastZeroMean::~NormalizeContrastZeroMean() {}

} /* namespace PV */
