/*
 * QuotientColProbe.cpp
 *
 *  Created on: Aug 12, 2015
 *      Author: pschultz
 */

#include "QuotientColProbe.hpp"
#include "columns/HyPerCol.hpp"
#include <limits>

namespace PV {

QuotientColProbe::QuotientColProbe() : ColProbe() { // Default constructor to be called by derived classes.
   // They should call QuotientColProbe::initialize from their own initialization routine
   // instead of calling a non-default constructor.
   initialize_base();
}  // end QuotientColProbe::QuotientColProbe(const char *)

QuotientColProbe::QuotientColProbe(const char * probename, HyPerCol * hc) : ColProbe() {
   initialize_base();
   initializeQuotientColProbe(probename, hc);
}

QuotientColProbe::~QuotientColProbe() {
   free(valueDescription);
   free(numerator);
   free(denominator);
   // Don't free numerProbe or denomProbe; they don't belong to the QuotientColProbe.
}

int QuotientColProbe::initialize_base() {
   valueDescription = NULL;
   numerator = NULL;
   denominator = NULL;
   numerProbe = NULL;
   denomProbe = NULL;
   return PV_SUCCESS;
}

int QuotientColProbe::initializeQuotientColProbe(const char * probename, HyPerCol * hc) {
   return ColProbe::initialize(probename, hc);
}

int QuotientColProbe::outputHeader() {
   if (outputStream) {
      output() << "Probe_name,time,index," << valueDescription;
   }
   return PV_SUCCESS;
}

int QuotientColProbe::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = ColProbe::ioParamsFillGroup(ioFlag);
   ioParam_valueDescription(ioFlag);
   ioParam_numerator(ioFlag);
   ioParam_denominator(ioFlag);
   return status;
}

void QuotientColProbe::ioParam_valueDescription(enum ParamsIOFlag ioFlag) {
   ioParamString(ioFlag, name, "valueDescription", &valueDescription, "value", true/*warnIfAbsent*/);
}

void QuotientColProbe::ioParam_numerator(enum ParamsIOFlag ioFlag) {
   ioParamStringRequired(ioFlag, name, "numerator", &numerator);
}

void QuotientColProbe::ioParam_denominator(enum ParamsIOFlag ioFlag) {
   ioParamStringRequired(ioFlag, name, "denominator", &denominator);
}

int QuotientColProbe::communicateInitInfo(std::shared_ptr<CommunicateInitInfoMessage const> message) {
   int status = ColProbe::communicateInitInfo(message);
   numerProbe = findProbe(numerator);
   denomProbe = findProbe(denominator);
   if (numerProbe==NULL || denomProbe==NULL) {
      status = PV_FAILURE;
      if (getCommunicator()->commRank()==0) {
         if (numerProbe==NULL) {
            pvErrorNoExit().printf("%s: numerator probe \"%s\" could not be found.\n", getDescription_c(), numerator);
         }
         if (denomProbe==NULL) {
            pvErrorNoExit().printf("%s: denominator probe \"%s\" could not be found.\n", getDescription_c(), denominator);
         }
      }
   }
   if (status == PV_SUCCESS) {
      int nNumValues = numerProbe->getNumValues();
      int dNumValues = denomProbe->getNumValues();
      if (nNumValues != dNumValues) {
         if (getCommunicator()->commRank()==0) {
            pvErrorNoExit().printf("%s: numerator probe \"%s\" and denominator probe \"%s\" have differing numbers of values (%d vs. %d)\n",
                  getDescription_c(), numerator, denominator, nNumValues, dNumValues);
         }
         MPI_Barrier(this->getCommunicator()->communicator());
         exit(EXIT_FAILURE);
      }
      status = setNumValues(nNumValues);
      if (status != PV_SUCCESS) {
         pvErrorNoExit().printf("%s: unable to allocate memory for %d values: %s\n",
               this->getDescription_c(), nNumValues, strerror(errno));
         exit(EXIT_FAILURE);
      }
   }
   if (status != PV_SUCCESS) {
      MPI_Barrier(getCommunicator()->communicator());
      exit(EXIT_FAILURE);
   }
   return status;
}

BaseProbe * QuotientColProbe::findProbe(char const * probeName) {
   for (int p=0; p<parent->numberOfBaseProbes(); p++) {
      BaseProbe * probe = parent->getBaseProbe(p);
      if (!strcmp(probe->getName(), probeName)) { return probe; }
   }
   // If you reach here, no probe with the given name was found.
   return NULL;
}

int QuotientColProbe::calcValues(double timeValue) {
   int numValues = this->getNumValues();
   double * valuesBuffer = getValuesBuffer();
   if (timeValue==parent->getStartTime()) {
      for (int b=0; b<numValues; b++) {
         valuesBuffer[b] = parent->getTimeScaleMin();
      }
      return PV_SUCCESS;
   }
   double n[numValues];
   numerProbe->getValues(timeValue, n);
   double d[numValues];
   denomProbe->getValues(timeValue, d);
   for (int b=0; b<numValues; b++) {
      valuesBuffer[b] = n[b]/d[b];
   }
   return PV_SUCCESS;
}

double QuotientColProbe::referenceUpdateTime() const {
   return parent->simulationTime();
}

int QuotientColProbe::outputState(double timevalue) {
   getValues(timevalue);
   if( this->getCommunicator()->commRank() != 0 ) return PV_SUCCESS;
   double * valuesBuffer = getValuesBuffer();
   int numValues = this->getNumValues();
   for(int b = 0; b < numValues; b++){
      if (isWritingToFile()) {
         output() << "\"" << valueDescription << "\",";
      }
      output() << timevalue << "," << b << "," << valuesBuffer[b] << "\n";
   }
   output().flush();
   return PV_SUCCESS;
}  // end QuotientColProbe::outputState(float, HyPerCol *)

}  // end namespace PV
