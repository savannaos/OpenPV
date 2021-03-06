/*
 * pv.cpp
 *
 */

#include "ResetStateOnTriggerTestProbe.hpp"
#include <columns/PV_Init.hpp>
#include <columns/buildandrun.hpp>

int customexit(HyPerCol *hc, int argc, char *argv[]);

int main(int argc, char *argv[]) {
   PV_Init pv_initObj(&argc, &argv, false /*do not allow unrecognized arguments*/);
   pv_initObj.registerKeyword(
         "ResetStateOnTriggerTestProbe", Factory::create<ResetStateOnTriggerTestProbe>);
   int status = buildandrun(&pv_initObj);
   return status == PV_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

int customexit(HyPerCol *hc, int argc, char *argv[]) {
   HyPerLayer *l = hc->getLayerFromName("TestLayer");
   FatalIf(!(l), "Test failed.\n");
   FatalIf(!(l->getNumProbes() == 1), "Test failed.\n");
   LayerProbe *p = l->getProbe(0);
   FatalIf(!(!strcmp(p->getName(), "TestProbe")), "Test failed.\n");
   ResetStateOnTriggerTestProbe *rsProbe = dynamic_cast<ResetStateOnTriggerTestProbe *>(p);
   FatalIf(!(rsProbe), "Test failed.\n");
   int status = PV_SUCCESS;
   if (rsProbe->getProbeStatus()) {
      if (hc->columnId() == 0) {
         ErrorLog().printf("%s failed at time %f\n", argv[0], rsProbe->getFirstFailureTime());
      }
      status = PV_FAILURE;
   }
   return status;
}
