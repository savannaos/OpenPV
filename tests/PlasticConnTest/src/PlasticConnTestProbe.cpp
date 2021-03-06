/*
 * PlasticConnTestProbe.cpp
 *
 *  Created on:
 *      Author: garkenyon
 */

#include "PlasticConnTestProbe.hpp"
#include <string.h>
#include <utils/PVLog.hpp>

namespace PV {

/**
 * @filename
 * @type
 * @msg
 */
PlasticConnTestProbe::PlasticConnTestProbe(const char *probename, HyPerCol *hc) {
   initialize(probename, hc);
}

int PlasticConnTestProbe::initialize(const char *probename, HyPerCol *hc) {
   errorPresent = false;
   return KernelProbe::initialize(probename, hc);
}
/**
 * @timef
 */
int PlasticConnTestProbe::outputState(double timed) {
   HyPerConn *c         = getTargetHyPerConn();
   Communicator *icComm = c->getParent()->getCommunicator();
   const int rcvProc    = 0;
   if (icComm->commRank() != rcvProc) {
      return PV_SUCCESS;
   }
   FatalIf(!(getTargetConn() != NULL), "Test failed.\n");
   outputStream->printf("    Time %f, %s:\n", timed, getTargetConn()->getDescription_c());
   const float *w  = c->get_wDataHead(getArbor(), getKernelIndex());
   const float *dw = c->get_dwDataHead(getArbor(), getKernelIndex());
   if (getOutputPlasticIncr() && dw == NULL) {
      Fatal().printf(
            "%s: %s has dKernelData(%d,%d) set to null.\n",
            getDescription_c(),
            getTargetConn()->getDescription_c(),
            getKernelIndex(),
            getArbor());
   }
   int nxp    = c->xPatchSize();
   int nyp    = c->yPatchSize();
   int nfp    = c->fPatchSize();
   int status = PV_SUCCESS;
   for (int k = 0; k < nxp * nyp * nfp; k++) {
      int x  = kxPos(k, nxp, nyp, nfp);
      int wx = (nxp - 1) / 2 - x; // assumes connection is one-to-one
      if (getOutputWeights()) {
         float wCorrect  = timed * wx;
         float wObserved = w[k];
         if (fabs(((double)(wObserved - wCorrect)) / timed) > 1e-4) {
            int y = kyPos(k, nxp, nyp, nfp);
            int f = featureIndex(k, nxp, nyp, nfp);
            outputStream->printf(
                  "        index %d (x=%d, y=%d, f=%d: w = %f, should be %f\n",
                  k,
                  x,
                  y,
                  f,
                  (double)wObserved,
                  (double)wCorrect);
         }
      }
      if (timed > 0 && getOutputPlasticIncr() && dw != NULL) {
         float dwCorrect  = wx;
         float dwObserved = dw[k];
         if (dwObserved != dwCorrect) {
            int y = kyPos(k, nxp, nyp, nfp);
            int f = featureIndex(k, nxp, nyp, nfp);
            outputStream->printf(
                  "        index %d (x=%d, y=%d, f=%d: dw = %f, should be %f\n",
                  k,
                  x,
                  y,
                  f,
                  (double)dwObserved,
                  (double)dwCorrect);
         }
      }
   }
   FatalIf(!(status == PV_SUCCESS), "Test failed.\n");
   if (status == PV_SUCCESS) {
      if (getOutputWeights()) {
         outputStream->printf("        All weights are correct.\n");
      }
      if (getOutputPlasticIncr()) {
         outputStream->printf("        All plastic increments are correct.\n");
      }
   }
   if (getOutputPatchIndices()) {
      patchIndices(c);
   }

   return PV_SUCCESS;
}

PlasticConnTestProbe::~PlasticConnTestProbe() {
   Communicator *icComm = getParent()->getCommunicator();
   if (icComm->commRank() == 0) {
      if (!errorPresent) {
         outputStream->printf("No errors detected\n");
      }
   }
}

} // end of namespace PV block
