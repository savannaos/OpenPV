/*
 * identicalBatchProbe.cpp
 * Author: slundquist
 */

#include "identicalBatchProbe.hpp"
#include <include/pv_arch.h>
#include <layers/HyPerLayer.hpp>
#include <string.h>
#include <utils/PVLog.hpp>

namespace PV {
identicalBatchProbe::identicalBatchProbe(const char *probeName, HyPerCol *hc) : StatsProbe() {
   initidenticalBatchProbe_base();
   initidenticalBatchProbe(probeName, hc);
}

int identicalBatchProbe::initidenticalBatchProbe_base() { return PV_SUCCESS; }

int identicalBatchProbe::initidenticalBatchProbe(const char *probeName, HyPerCol *hc) {
   return initStatsProbe(probeName, hc);
}

void identicalBatchProbe::ioParam_buffer(enum ParamsIOFlag ioFlag) { requireType(BufActivity); }

// 2 tests: max difference can be 5e-4, max std is 5e-5
int identicalBatchProbe::outputState(double timed) {
   int status            = StatsProbe::outputState(timed);
   const PVLayerLoc *loc = getTargetLayer()->getLayerLoc();
   const float *A        = getTargetLayer()->getActivity();
   int numExtNeurons     = getTargetLayer()->getNumExtended();
   for (int i = 0; i < numExtNeurons; i++) {
      float checkVal = A[i];
      for (int b = 0; b < loc->nbatch; b++) {
         const float *ABatch = A + b * getTargetLayer()->getNumExtended();
         float diff          = fabsf(checkVal - ABatch[i]);
         if (diff > 1e-4f) {
            Fatal() << "Difference at neuron " << i << ", batch 0: " << checkVal << " batch " << b
                    << ": " << ABatch[i] << "\n";
         }
         FatalIf(!(diff <= 1e-4f), "Test failed.\n");
      }
   }
   return status;
}

} // end namespace PV
