/* CloneKernelConn.cpp
 *
 * Created on: May 23, 2011
 *     Author: peteschultz
 */

#include "CloneKernelConn.hpp"

namespace PV {

CloneKernelConn::CloneKernelConn() : CloneConn() {}

CloneKernelConn::CloneKernelConn(const char *name, HyPerCol *hc) { initialize(name, hc); }

int CloneKernelConn::initialize(const char *name, HyPerCol *hc) {
   int status = CloneConn::initialize(name, hc);
   if (hc->columnId() == 0) {
      Fatal().printf(
            "%s: CloneKernelConn is obsolete.  Use CloneConn with sharedWeights=true.\n",
            getDescription_c());
   }
   MPI_Barrier(parent->getCommunicator()->communicator());
   exit(EXIT_FAILURE);
   return status;
}

CloneKernelConn::~CloneKernelConn() {}

} // end namespace PV
