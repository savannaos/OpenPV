/*
 * BaseProbe.cpp
 *
 *  Created on: Mar 7, 2009
 *      Author: rasmussn
 */

#include "BaseProbe.hpp"
#include "ColumnEnergyProbe.hpp"
#include "layers/HyPerLayer.hpp"
#include <float.h>
#include <limits>

namespace PV {

BaseProbe::BaseProbe() {
   initialize_base();
   // Derived classes of BaseProbe should call BaseProbe::initialize themselves.
}

BaseProbe::~BaseProbe() {
   delete outputStream;
   free(targetName);
   targetName = NULL;
   free(msgparams);
   msgparams = NULL;
   free(msgstring);
   msgstring = NULL;
   free(probeOutputFilename);
   probeOutputFilename = NULL;
   if (triggerLayerName) {
      free(triggerLayerName);
      triggerLayerName = NULL;
   }
   free(energyProbe);
   free(probeValues);
}

int BaseProbe::initialize_base() {
   outputStream        = NULL;
   targetName          = NULL;
   msgparams           = NULL;
   msgstring           = NULL;
   textOutputFlag      = true;
   probeOutputFilename = NULL;
   triggerFlag         = false;
   triggerLayerName    = NULL;
   triggerLayer        = NULL;
   triggerOffset       = 0;
   energyProbe         = NULL;
   coefficient         = 1.0;
   numValues           = 0;
   probeValues         = NULL;
   lastUpdateTime      = 0.0;
   writingToFile       = false;
   return PV_SUCCESS;
}

/**
 * @filename
 * @layer
 */
int BaseProbe::initialize(const char *probeName, HyPerCol *hc) {
   int status = BaseObject::initialize(probeName, hc);
   if (status != PV_SUCCESS) {
      return status;
   }
   ioParams(PARAMS_IO_READ);
   // Add probe to list of probes
   parent->addBaseProbe(this); // Adds probe to HyPerCol.  If needed, probe will be attached to
   // layer or connection during communicateInitInfo
   status = initNumValues();
   return status;
}

int BaseProbe::ioParams(enum ParamsIOFlag ioFlag) {
   parent->ioParamsStartGroup(ioFlag, name);
   ioParamsFillGroup(ioFlag);
   parent->ioParamsFinishGroup(ioFlag);
   return PV_SUCCESS;
}

int BaseProbe::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   ioParam_targetName(ioFlag);
   ioParam_message(ioFlag);
   ioParam_textOutputFlag(ioFlag);
   ioParam_probeOutputFile(ioFlag);
   ioParam_triggerLayerName(ioFlag);
   ioParam_triggerFlag(ioFlag);
   ioParam_triggerOffset(ioFlag);
   ioParam_energyProbe(ioFlag);
   ioParam_coefficient(ioFlag);
   return PV_SUCCESS;
}

void BaseProbe::ioParam_targetName(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamStringRequired(ioFlag, name, "targetName", &targetName);
}

void BaseProbe::ioParam_message(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamString(
         ioFlag, name, "message", &msgparams, NULL, false /*warnIfAbsent*/);
   if (ioFlag == PARAMS_IO_READ) {
      initMessage(msgparams);
   }
}

void BaseProbe::ioParam_energyProbe(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamString(
         ioFlag, name, "energyProbe", &energyProbe, NULL, false /*warnIfAbsent*/);
}

void BaseProbe::ioParam_coefficient(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "energyProbe"));
   if (energyProbe && energyProbe[0]) {
      parent->parameters()->ioParamValue(
            ioFlag, name, "coefficient", &coefficient, coefficient, true /*warnIfAbsent*/);
   }
}

void BaseProbe::ioParam_textOutputFlag(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(
         ioFlag, name, "textOutputFlag", &textOutputFlag, textOutputFlag);
}

void BaseProbe::ioParam_probeOutputFile(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "textOutputFlag"));
   if (textOutputFlag) {
      parent->parameters()->ioParamString(
            ioFlag, name, "probeOutputFile", &probeOutputFilename, NULL, false /*warnIfAbsent*/);
   }
}

void BaseProbe::ioParam_triggerLayerName(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamString(
         ioFlag, name, "triggerLayerName", &triggerLayerName, NULL, false /*warnIfAbsent*/);
   if (ioFlag == PARAMS_IO_READ) {
      triggerFlag = (triggerLayerName != NULL && triggerLayerName[0] != '\0');
   }
}

// triggerFlag was deprecated Oct 7, 2015.
// Setting triggerLayerName to a nonempty string has the effect of
// triggerFlag=true, and
// setting triggerLayerName to NULL or "" has the effect of triggerFlag=false.
// While triggerFlag is being deprecated, it is an error for triggerFlag to be
// false
// and triggerLayerName to be a nonempty string.
void BaseProbe::ioParam_triggerFlag(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "triggerLayerName"));
   if (ioFlag == PARAMS_IO_READ && parent->parameters()->present(name, "triggerFlag")) {
      bool flagFromParams = false;
      parent->parameters()->ioParamValue(
            ioFlag, name, "triggerFlag", &flagFromParams, flagFromParams);
      if (parent->columnId() == 0) {
         WarnLog(triggerFlagDeprecated);
         triggerFlagDeprecated.printf("%s: triggerFlag has been deprecated.\n", getDescription_c());
         triggerFlagDeprecated.printf(
               "   If triggerLayerName is a nonempty "
               "string, triggering will be on;\n");
         triggerFlagDeprecated.printf(
               "   if triggerLayerName is empty or null, triggering will be off.\n");
         if (flagFromParams != triggerFlag) {
            ErrorLog(triggerFlagError);
            triggerFlagError.printf("%s: triggerLayerName=", getDescription_c());
            if (triggerLayerName) {
               triggerFlagError.printf("\"%s\"", triggerLayerName);
            }
            else {
               triggerFlagError.printf("NULL");
            }
            triggerFlagError.printf(
                  " implies triggerFlag=%s but triggerFlag was set in params to %s\n",
                  triggerFlag ? "true" : "false",
                  flagFromParams ? "true" : "false");
         }
      }
   }
}

void BaseProbe::ioParam_triggerOffset(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "triggerFlag"));
   if (triggerFlag) {
      parent->parameters()->ioParamValue(
            ioFlag, name, "triggerOffset", &triggerOffset, triggerOffset);
      if (triggerOffset < 0) {
         Fatal().printf(
               "%s \"%s\" error in rank %d process: TriggerOffset (%f) "
               "must be positive\n",
               parent->parameters()->groupKeywordFromName(name),
               name,
               parent->columnId(),
               triggerOffset);
      }
   }
}

int BaseProbe::initOutputStream(const char *filename) {
   if (parent->columnId() == 0) {
      if (filename != NULL) {
         std::string path("");
         if (filename[0] != '/') {
            path += parent->getOutputPath();
            path += "/";
         }
         path += filename;
         bool append                  = parent->getCheckpointReadFlag();
         std::ios_base::openmode mode = std::ios_base::out;
         if (append) {
            mode |= std::ios_base::app;
         }
         outputStream  = new FileStream(path.c_str(), mode, parent->getVerifyWrites());
         writingToFile = true;
      }
      else {
         outputStream  = new PrintStream(PV::getOutputStream());
         writingToFile = false;
      }
   }
   else {
      outputStream = NULL; // Only root process writes; if other processes need
      // something written it
      // should be sent to root.
      // Derived classes for which it makes sense for a different process to do
      // the file i/o should
      // override initOutputStream
   }
   return PV_SUCCESS;
}

int BaseProbe::initNumValues() { return setNumValues(parent->getNBatch()); }

int BaseProbe::setNumValues(int n) {
   int status = PV_SUCCESS;
   if (n > 0) {
      double *newValuesBuffer = (double *)realloc(probeValues, (size_t)n * sizeof(*probeValues));
      if (newValuesBuffer != NULL) {
         // realloc() succeeded
         probeValues = newValuesBuffer;
         numValues   = n;
      }
      else {
         // realloc() failed
         status = PV_FAILURE;
      }
   }
   else {
      free(probeValues);
      probeValues = NULL;
   }
   return status;
}

int BaseProbe::communicateInitInfo() {
   int status = PV_SUCCESS;

   // Set up triggering.
   if (triggerFlag) {
      triggerLayer = parent->getLayerFromName(triggerLayerName);
      if (triggerLayer == NULL) {
         if (parent->columnId() == 0) {
            ErrorLog().printf(
                  "%s \"%s\": triggerLayer \"%s\" is not a layer in the HyPerCol.\n",
                  parent->parameters()->groupKeywordFromName(name),
                  name,
                  triggerLayerName);
         }
         MPI_Barrier(parent->getCommunicator()->communicator());
         exit(EXIT_FAILURE);
      }
   }

   // Add the probe to the ColumnEnergyProbe, if there is one.
   if (energyProbe && energyProbe[0]) {
      BaseProbe *baseprobe     = getParent()->getBaseProbeFromName(energyProbe);
      ColumnEnergyProbe *probe = dynamic_cast<ColumnEnergyProbe *>(baseprobe);
      if (probe == NULL) {
         if (getParent()->columnId() == 0) {
            ErrorLog().printf(
                  "%s \"%s\": energyProbe \"%s\" is not a ColumnEnergyProbe in the "
                  "column.\n",
                  getParent()->parameters()->groupKeywordFromName(getName()),
                  getName(),
                  energyProbe);
         }
         MPI_Barrier(getParent()->getCommunicator()->communicator());
         exit(EXIT_FAILURE);
      }
      status = probe->addTerm(this);
   }
   return status;
}

int BaseProbe::allocateDataStructures() {
   int status = PV_SUCCESS;

   // Set up output stream
   status = initOutputStream(probeOutputFilename);

   return PV_SUCCESS;
}

int BaseProbe::initMessage(const char *msg) {
   assert(msgstring == NULL);
   int status = PV_SUCCESS;
   if (msg != NULL && msg[0] != '\0') {
      size_t msglen   = strlen(msg);
      this->msgstring = (char *)calloc(
            msglen + 2,
            sizeof(char)); // Allocate room for colon plus null terminator
      if (this->msgstring) {
         memcpy(this->msgstring, msg, msglen);
         this->msgstring[msglen]     = ':';
         this->msgstring[msglen + 1] = '\0';
      }
   }
   else {
      this->msgstring = (char *)calloc(1, sizeof(char));
      if (this->msgstring) {
         this->msgstring[0] = '\0';
      }
   }
   if (!this->msgstring) {
      ErrorLog().printf(
            "%s \"%s\": Unable to allocate memory for probe's message.\n",
            parent->parameters()->groupKeywordFromName(name),
            name);
      status = PV_FAILURE;
   }
   assert(status == PV_SUCCESS);
   return status;
}

bool BaseProbe::needUpdate(double simTime, double dt) {
   if (triggerFlag) {
      return triggerLayer->needUpdate(simTime + triggerOffset, dt);
   }
   return true;
}

int BaseProbe::getValues(double timevalue) {
   int status = PV_SUCCESS;
   if (needRecalc(timevalue)) {
      status = calcValues(timevalue);
      if (status == PV_SUCCESS) {
         lastUpdateTime = referenceUpdateTime();
      }
   }
   return status;
}

int BaseProbe::getValues(double timevalue, double *values) {
   int status = getValues(timevalue);
   if (status == PV_SUCCESS) {
      memcpy(values, probeValues, sizeof(*probeValues) * (size_t)getNumValues());
   }
   return status;
}

int BaseProbe::getValues(double timevalue, std::vector<double> *valuesVector) {
   valuesVector->resize(this->getNumValues());
   return getValues(timevalue, &valuesVector->front());
}

double BaseProbe::getValue(double timevalue, int index) {
   if (index < 0 || index >= getNumValues()) {
      return std::numeric_limits<double>::signaling_NaN();
   }
   else {
      int status = PV_SUCCESS;
      if (needRecalc(timevalue)) {
         status = getValues(timevalue);
      }
      if (status != PV_SUCCESS) {
         return std::numeric_limits<double>::signaling_NaN();
      }
   }
   return probeValues[index];
}

int BaseProbe::outputStateWrapper(double timef, double dt) {
   int status = PV_SUCCESS;
   if (textOutputFlag && needUpdate(timef, dt)) {
      status = outputState(timef);
   }
   return status;
}

} // namespace PV
