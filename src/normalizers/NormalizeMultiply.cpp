/*
 * NormalizeMultiply.cpp
 *
 *  Created on: Oct 24, 2014
 *      Author: pschultz
 */

#include "NormalizeMultiply.hpp"

namespace PV {

NormalizeMultiply::NormalizeMultiply(const char *name, HyPerCol *hc) {
   initialize_base();
   initialize(name, hc);
}

NormalizeMultiply::NormalizeMultiply() { initialize_base(); }

int NormalizeMultiply::initialize_base() {
   rMinX                        = 0.0f;
   rMinY                        = 0.0f;
   nonnegativeConstraintFlag    = false;
   normalize_cutoff             = 0.0f;
   normalizeFromPostPerspective = false;
   return PV_SUCCESS;
}

int NormalizeMultiply::initialize(const char *name, HyPerCol *hc) {
   int status = NormalizeBase::initialize(name, hc);
   return status;
}

int NormalizeMultiply::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = NormalizeBase::ioParamsFillGroup(ioFlag);
   ioParam_rMinX(ioFlag);
   ioParam_rMinY(ioFlag);
   ioParam_nonnegativeConstraintFlag(ioFlag);
   ioParam_normalize_cutoff(ioFlag);
   ioParam_normalizeFromPostPerspective(ioFlag);
   return status;
}

void NormalizeMultiply::ioParam_rMinX(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(ioFlag, name, "rMinX", &rMinX, rMinX);
}

void NormalizeMultiply::ioParam_rMinY(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(ioFlag, name, "rMinY", &rMinY, rMinY);
}

void NormalizeMultiply::ioParam_nonnegativeConstraintFlag(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(
         ioFlag,
         name,
         "nonnegativeConstraintFlag",
         &nonnegativeConstraintFlag,
         nonnegativeConstraintFlag);
}

void NormalizeMultiply::ioParam_normalize_cutoff(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(
         ioFlag, name, "normalize_cutoff", &normalize_cutoff, normalize_cutoff);
}

void NormalizeMultiply::ioParam_normalizeFromPostPerspective(enum ParamsIOFlag ioFlag) {
   if (ioFlag == PARAMS_IO_READ
       && !parent->parameters()->present(name, "normalizeFromPostPerspective")
       && parent->parameters()->present(name, "normalize_arbors_individually")) {
      if (parent->columnId() == 0) {
         WarnLog().printf(
               "Normalizer \"%s\": parameter name normalizeTotalToPost is deprecated.  Use "
               "normalizeFromPostPerspective.\n",
               name);
      }
      normalizeFromPostPerspective = parent->parameters()->value(name, "normalizeTotalToPost");
      return;
   }
   parent->parameters()->ioParamValue(
         ioFlag,
         name,
         "normalizeFromPostPerspective",
         &normalizeFromPostPerspective,
         false /*default value*/,
         true /*warnIfAbsent*/);
}

int NormalizeMultiply::normalizeWeights() {
   int status = PV_SUCCESS;

   // All connections in the group must have the same values of sharedWeights, numArbors, and
   // numDataPatches
   HyPerConn *conn0 = connectionList[0];
   for (int c = 1; c < numConnections; c++) {
      HyPerConn *conn = connectionList[c];
      // Do we need to require sharedWeights be the same for all connections in the group?
      if (conn->usingSharedWeights() != conn0->usingSharedWeights()) {
         if (parent->columnId() == 0) {
            ErrorLog().printf(
                  "%s: All connections in the normalization group must have the same sharedWeights "
                  "(%s has %d; %s has %d).\n",
                  this->getDescription_c(),
                  conn0->getDescription_c(),
                  conn0->usingSharedWeights(),
                  conn->getDescription_c(),
                  conn->usingSharedWeights());
         }
         status = PV_FAILURE;
      }
      if (conn->numberOfAxonalArborLists() != conn0->numberOfAxonalArborLists()) {
         if (parent->columnId() == 0) {
            ErrorLog().printf(
                  "%s: All connections in the normalization group must have the same number of "
                  "arbors (%s has %d; %s has %d).\n",
                  this->getDescription_c(),
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
                  this->getDescription_c(),
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

   // Apply rMinX and rMinY
   if (rMinX > 0.5f && rMinY > 0.5f) {
      for (int c = 0; c < numConnections; c++) {
         HyPerConn *conn          = connectionList[c];
         int num_arbors           = conn->numberOfAxonalArborLists();
         int num_patches          = conn->getNumDataPatches();
         int num_weights_in_patch = conn->xPatchSize() * conn->yPatchSize() * conn->fPatchSize();
         for (int arbor = 0; arbor < num_arbors; arbor++) {
            float *dataPatchStart = conn->get_wDataStart(arbor);
            for (int patchindex = 0; patchindex < num_patches; patchindex++) {
               applyRMin(
                     dataPatchStart + patchindex * num_weights_in_patch,
                     rMinX,
                     rMinY,
                     conn->xPatchSize(),
                     conn->yPatchSize(),
                     conn->xPatchStride(),
                     conn->yPatchStride());
            }
         }
      }
   }

   // Apply nonnegativeConstraintFlag
   if (nonnegativeConstraintFlag) {
      for (int c = 0; c < numConnections; c++) {
         HyPerConn *conn          = connectionList[c];
         int num_arbors           = conn->numberOfAxonalArborLists();
         int num_patches          = conn->getNumDataPatches();
         int num_weights_in_patch = conn->xPatchSize() * conn->yPatchSize() * conn->fPatchSize();
         int num_weights_in_arbor = num_patches * num_weights_in_patch;
         for (int arbor = 0; arbor < num_arbors; arbor++) {
            float *dataPatchStart = conn->get_wDataStart(arbor);
            for (int weightindex = 0; weightindex < num_weights_in_arbor; weightindex++) {
               float *w = &dataPatchStart[weightindex];
               if (*w < 0) {
                  *w = 0;
               }
            }
         }
      }
   }

   // Apply normalize_cutoff
   if (normalize_cutoff > 0) {
      float max = 0.0f;
      for (int c = 0; c < numConnections; c++) {
         HyPerConn *conn          = connectionList[c];
         int num_arbors           = conn->numberOfAxonalArborLists();
         int num_patches          = conn->getNumDataPatches();
         int num_weights_in_patch = conn->xPatchSize() * conn->yPatchSize() * conn->fPatchSize();
         for (int arbor = 0; arbor < num_arbors; arbor++) {
            float *dataStart = conn->get_wDataStart(arbor);
            for (int patchindex = 0; patchindex < num_patches; patchindex++) {
               accumulateMaxAbs(
                     dataStart + patchindex * num_weights_in_patch, num_weights_in_patch, &max);
            }
         }
      }
      for (int c = 0; c < numConnections; c++) {
         HyPerConn *conn          = connectionList[c];
         int num_arbors           = conn->numberOfAxonalArborLists();
         int num_patches          = conn->getNumDataPatches();
         int num_weights_in_patch = conn->xPatchSize() * conn->yPatchSize() * conn->fPatchSize();
         for (int arbor = 0; arbor < num_arbors; arbor++) {
            float *dataStart = conn->get_wDataStart(arbor);
            for (int patchindex = 0; patchindex < num_patches; patchindex++) {
               applyThreshold(
                     dataStart + patchindex * num_weights_in_patch, num_weights_in_patch, max);
            }
         }
      }
   }

   return PV_SUCCESS;
}

/**
 * Sets all weights in a patch whose absolute value is below a certain value, to zero
 * dataPatchStart is a pointer to a buffer of weights
 * weights_in_patch is the number of weights in the dataPatchStart buffer
 * wMax defines the threshold.  If |w| < wMax * normalize_cutoff, the weight will be zeroed.
 */
int NormalizeMultiply::applyThreshold(float *dataPatchStart, int weights_in_patch, float wMax) {
   assert(normalize_cutoff > 0); // Don't call this routine unless normalize_cutoff was set
   float threshold = wMax * normalize_cutoff;
   for (int k = 0; k < weights_in_patch; k++) {
      if (fabsf(dataPatchStart[k]) < threshold)
         dataPatchStart[k] = 0;
   }
   return PV_SUCCESS;
}

// dataPatchStart points to head of full-sized patch
// rMinX, rMinY are the minimum radii from the center of the patch,
// all weights inside (non-inclusive) of this radius are set to zero
// the diameter of the central exclusion region is truncated to the nearest integer value, which may
// be zero
int NormalizeMultiply::applyRMin(
      float *dataPatchStart,
      float rMinX,
      float rMinY,
      int nxp,
      int nyp,
      int xPatchStride,
      int yPatchStride) {
   if (rMinX == 0 && rMinY == 0)
      return PV_SUCCESS;
   int fullWidthX        = floor(2 * rMinX);
   int fullWidthY        = floor(2 * rMinY);
   int offsetX           = ceil((nxp - fullWidthX) / 2.0);
   int offsetY           = ceil((nyp - fullWidthY) / 2.0);
   int widthX            = nxp - 2 * offsetX;
   int widthY            = nyp - 2 * offsetY;
   float *rMinPatchStart = dataPatchStart + offsetY * yPatchStride + offsetX * xPatchStride;
   int weights_in_row    = xPatchStride * widthX;
   for (int ky = 0; ky < widthY; ky++) {
      for (int k = 0; k < weights_in_row; k++) {
         rMinPatchStart[k] = 0;
      }
      rMinPatchStart += yPatchStride;
   }
   return PV_SUCCESS;
}

NormalizeMultiply::~NormalizeMultiply() {}

} /* namespace PV */
