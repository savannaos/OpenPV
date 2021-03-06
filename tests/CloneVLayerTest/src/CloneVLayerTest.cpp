/*
 * pv.cpp
 *
 */

#include <columns/buildandrun.hpp>

int customexit(HyPerCol *hc, int argc, char *argv[]);

int main(int argc, char *argv[]) {
   PV_Init initObj(&argc, &argv, false /*allowUnrecognizedArguments*/);
   if (initObj.getParams() == nullptr) {
      initObj.setParams("input/CloneVLayerTest.params");
   }

   int status;
   status = rebuildandrun(&initObj, NULL, &customexit);
   return status == PV_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

int customexit(HyPerCol *hc, int argc, char *argv[]) {
   int check_clone_id   = -1;
   int check_sigmoid_id = -1;
   for (int k = 0; k < hc->numberOfLayers(); k++) {
      if (!strcmp(hc->getLayer(k)->getName(), "CheckClone")) {
         FatalIf(!(check_clone_id < 0), "Test failed.\n");
         check_clone_id = k;
      }
      if (!strcmp(hc->getLayer(k)->getName(), "CheckSigmoid")) {
         FatalIf(!(check_sigmoid_id < 0), "Test failed.\n");
         check_sigmoid_id = k;
      }
   }

   int N;

   HyPerLayer *check_clone_layer = hc->getLayer(check_clone_id);
   FatalIf(!(check_clone_layer != NULL), "Test failed.\n");
   const float *check_clone_layer_data = check_clone_layer->getLayerData();
   N                                   = check_clone_layer->getNumExtended();

   for (int k = 0; k < N; k++) {
      FatalIf(!(fabsf(check_clone_layer_data[k]) < 1e-6f), "Test failed.\n");
   }

   HyPerLayer *check_sigmoid_layer = hc->getLayer(check_sigmoid_id);
   FatalIf(!(check_sigmoid_layer != NULL), "Test failed.\n");
   const float *check_sigmoid_layer_data = check_sigmoid_layer->getLayerData();
   N                                     = check_sigmoid_layer->getNumExtended();

   for (int k = 0; k < N; k++) {
      FatalIf(!(fabsf(check_sigmoid_layer_data[k]) < 1e-6f), "Test failed.\n");
   }

   if (hc->columnId() == 0) {
      InfoLog().printf("%s passed.\n", argv[0]);
   }
   return PV_SUCCESS;
}
