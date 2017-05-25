/*
 * ColProbe.cpp
 *
 *  Created on: Nov 25, 2010
 *      Author: pschultz
 */

#include "ColProbe.hpp"
#include "columns/HyPerCol.hpp"

namespace PV {

ColProbe::ColProbe() { // Default constructor to be called by derived classes.
   // They should call ColProbe::initialize from their own initialization routine
   // instead of calling a non-default constructor.
   initialize_base();
}

ColProbe::ColProbe(const char *probeName, HyPerCol *hc) {
   initialize_base();
   initialize(probeName, hc);
}

ColProbe::~ColProbe() {}

int ColProbe::initialize_base() {
   parent = NULL;
   return PV_SUCCESS;
}

int ColProbe::initialize(const char *probeName, HyPerCol *hc) {
   int status = BaseProbe::initialize(probeName, hc);
   if (status == PV_SUCCESS) {
      this->parent->insertProbe(this);
   }
   return status;
}

int ColProbe::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = PV::BaseProbe::ioParamsFillGroup(ioFlag);
   return status;
}

void ColProbe::ioParam_targetName(enum ParamsIOFlag ioFlag) {
   if (ioFlag == PARAMS_IO_READ) {
      targetName = strdup("");
   }
}

int ColProbe::initOutputStream(const char *filename, Checkpointer *checkpointer) {
   int status = BaseProbe::initOutputStream(filename, checkpointer);
   if (status != PV_SUCCESS) {
      status = outputHeader();
   }
   return status;
}

int ColProbe::communicateInitInfo(CommunicateInitInfoMessage const *message) {
   int status = BaseProbe::communicateInitInfo(message);
   return status;
}

} // end namespace PV
