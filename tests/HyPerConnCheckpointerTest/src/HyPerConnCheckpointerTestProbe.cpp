/*
 * HyPerConnCheckpointerTestProbe.cpp
 *
 *  Created on: Jan 5, 2017
 *      Author: pschultz
 */

#include "HyPerConnCheckpointerTestProbe.hpp"
#include <cmath>
#include <utils/BufferUtilsMPI.hpp>

HyPerConnCheckpointerTestProbe::HyPerConnCheckpointerTestProbe() { initialize_base(); }

HyPerConnCheckpointerTestProbe::HyPerConnCheckpointerTestProbe(
      const char *probeName,
      PV::HyPerCol *hc) {
   initialize_base();
   initialize(probeName, hc);
}

HyPerConnCheckpointerTestProbe::~HyPerConnCheckpointerTestProbe() {}

int HyPerConnCheckpointerTestProbe::initialize_base() { return PV_SUCCESS; }

int HyPerConnCheckpointerTestProbe::initialize(const char *probeName, PV::HyPerCol *hc) {
   int status = PV::ColProbe::initialize(probeName, hc);
   FatalIf(parent->getDeltaTime() != 1.0, "This test assumes that the HyPerCol dt is 1.0.\n");
   mEffectiveStartTime = parent->getStartTime();
   return status;
}

void HyPerConnCheckpointerTestProbe::ioParam_textOutputFlag(enum PV::ParamsIOFlag ioFlag) {
   ColProbe::ioParam_textOutputFlag(ioFlag);
   if (ioFlag == PV::PARAMS_IO_READ && !getTextOutputFlag()) {
      if (parent->getCommunicator()->globalCommRank() == 0) {
         ErrorLog()
               << getDescription()
               << ": HyPerConnCheckpointerTestProbe requires textOutputFlag to be set to true.\n";
      }
   }
}

int HyPerConnCheckpointerTestProbe::communicateInitInfo() {
   int status = PV::ColProbe::communicateInitInfo();
   FatalIf(
         status != PV_SUCCESS, "%s failed in ColProbe::communicateInitInfo\n", getDescription_c());

   if (initInputLayer() == PV_POSTPONE) { return PV_POSTPONE; }
   if (initOutputLayer() == PV_POSTPONE) { return PV_POSTPONE; }
   if (initConnection() == PV_POSTPONE) { return PV_POSTPONE; }
   return status;
}

int HyPerConnCheckpointerTestProbe::initInputLayer() {
   mInputLayer = dynamic_cast<PV::InputLayer *>(parent->getLayerFromName("Input"));
   FatalIf(mInputLayer == nullptr, "column does not have an InputLayer named \"Input\".\n");
   if (checkCommunicatedFlag(mInputLayer) == PV_POSTPONE) { return PV_POSTPONE; }

   FatalIf(
         mInputLayer->getDisplayPeriod() != 4.0,
         "This test assumes that the display period is 4 (should really not be hard-coded.\n");
}

int HyPerConnCheckpointerTestProbe::initOutputLayer() {
   mOutputLayer = parent->getLayerFromName("Output");
   FatalIf(mOutputLayer == nullptr, "column does not have a HyPerLayer named \"Output\".\n");
   if (checkCommunicatedFlag(mOutputLayer) == PV_POSTPONE) { return PV_POSTPONE; }
}

int HyPerConnCheckpointerTestProbe::initConnection() {
   mConnection = dynamic_cast<PV::HyPerConn *>(parent->getConnFromName("InputToOutput"));
   FatalIf(mConnection == nullptr, "column does not have a HyPerConn named \"InputToOutput\".\n");
   if (checkCommunicatedFlag(mConnection) == PV_POSTPONE) { return PV_POSTPONE; }

   FatalIf(
         mConnection->numberOfAxonalArborLists() != 1,
         "This test assumes that the connection has only 1 arbor.\n");
   FatalIf(mConnection->getDelay(0) != 0.0, "This test assumes that the connection has zero delay.\n");
   FatalIf(mConnection->xPatchSize() != 1, "This test assumes that the connection has nxp==1.\n");
   FatalIf(mConnection->yPatchSize() != 1, "This test assumes that the connection has nyp==1.\n");
   FatalIf(mConnection->fPatchSize() != 1, "This test assumes that the connection has nfp==1.\n");
}

int HyPerConnCheckpointerTestProbe::checkCommunicatedFlag(PV::BaseObject* dependencyObject) {
   if (!dependencyObject->getInitInfoCommunicatedFlag()) {
      if (parent->getCommunicator()->commRank() == 0) {
         InfoLog().printf(
               "%s must wait until \"%s\" has finished its communicateInitInfo stage.\n",
               getDescription_c(),
               dependencyObject->getName());
      }
      return PV_POSTPONE;
   }
   else {
      return PV_SUCCESS;
   }
}

int HyPerConnCheckpointerTestProbe::readStateFromCheckpoint(PV::Checkpointer *checkpointer) {
   PV::Checkpointer::TimeInfo timeInfo;
   PV::CheckpointEntryData<PV::Checkpointer::TimeInfo> timeInfoCheckpointEntry(
         std::string("timeinfo"),
         parent->getCommunicator(),
         &timeInfo,
         (size_t)1,
         true /*broadcast*/);
   std::string initializeFromCheckpointDir(checkpointer->getInitializeFromCheckpointDir());
   timeInfoCheckpointEntry.read(initializeFromCheckpointDir, nullptr);

   mEffectiveStartTime = timeInfo.mSimTime - parent->getStartTime();

   return PV_SUCCESS;
}

int HyPerConnCheckpointerTestProbe::calcUpdateNumber(double timevalue) {
   int const step = (int)std::nearbyint(timevalue - parent->getStartTime());
   pvAssert(step >= 0);
   int const updateNumber = (step + 3) / 4; // integer division
   return updateNumber;
}

void HyPerConnCheckpointerTestProbe::initializeCorrectValues(double timevalue) {
   pvAssert(timevalue >= parent->getStartTime());
   if (timevalue == parent->getStartTime()) {
      mCorrectWeightValue      = 1.0f;
      mCorrectInputLayerValue  = 1.0f;
      mCorrectOutputLayerValue = 2.0f;
      mUpdateNumber            = 0;
   }
   else {
      mUpdateNumber           = 1;
      mCorrectWeightValue     = 3.0f;
      mCorrectInputLayerValue = 1.0f;
      // Note: the for-loop below will update mCorrectInputLayerValue to
      // (float)updateNumber. The reason for setting it to 1.0f here is
      // so that after every call of nextValues, the mCorrect* values are
      // all consistent with the updateNumber.
      mCorrectOutputLayerValue = 3.0f;

      int const updateNumber = calcUpdateNumber(timevalue);
      for (int j = 2; j < updateNumber; j++) {
         nextValues(j); // updates mCorrectWeightValue and mCorrectOutputLayerValue.
      }
      // Don't update for the current updateNumber; this will happen in outputState.
   }
}

int HyPerConnCheckpointerTestProbe::outputState(double timevalue) {
   double effectiveTime = timevalue + mEffectiveStartTime;
   if (!mValuesSet) {
      initializeCorrectValues(effectiveTime);
      mValuesSet = true;
   }
   int const updateNumber = calcUpdateNumber(effectiveTime);
   if (updateNumber > mUpdateNumber) {
      nextValues(updateNumber);
   }

   bool failed = false;

   failed |= verifyLayer(mInputLayer, mCorrectInputLayerValue, timevalue);
   failed |= verifyLayer(mOutputLayer, mCorrectOutputLayerValue, timevalue);
   failed |= verifyConnection(mConnection, mCorrectWeightValue, timevalue);

   if (failed) {
      std::string errorMsg(getDescription() + " failed at t = " + std::to_string(timevalue) + "\n");
      if (outputStream) {
         outputStream->printf(errorMsg.c_str());
      }
      if (isWritingToFile()) { // print error message to screen/log file as well.
         ErrorLog() << errorMsg;
      }
      mTestFailed = true;
   }
   else {
      if (outputStream) {
         outputStream->printf(
               "%s found all correct values at time %f\n", getDescription_c(), timevalue);
      }
   }
}

void HyPerConnCheckpointerTestProbe::nextValues(int updateNumber) {
   mCorrectWeightValue += mCorrectInputLayerValue * mCorrectOutputLayerValue;
   mCorrectInputLayerValue  = (float)updateNumber;
   mCorrectOutputLayerValue = mCorrectInputLayerValue * mCorrectWeightValue;
   mUpdateNumber++;
}

bool HyPerConnCheckpointerTestProbe::verifyLayer(
      PV::HyPerLayer *layer,
      float correctValue,
      double timevalue) {
   bool failed = false;

   PVLayerLoc const *inputLoc = layer->getLayerLoc();
   PVHalo const *inputHalo    = &inputLoc->halo;
   int const inputNxExt       = inputLoc->nx + inputHalo->lt + inputHalo->rt;
   int const inputNyExt       = inputLoc->ny + inputHalo->dn + inputHalo->up;
   PV::Buffer<float> localBuffer(layer->getLayerData(0), inputNxExt, inputNyExt, inputLoc->nf);
   PV::Communicator *comm = parent->getCommunicator();
   PV::Buffer<float> globalBuffer =
         PV::BufferUtils::gather(comm, localBuffer, inputLoc->nx, inputLoc->ny);
   if (comm->commRank() == 0) {
      globalBuffer.crop(inputLoc->nxGlobal, inputLoc->nyGlobal, PV::Buffer<float>::CENTER);
      std::vector<float> globalVector = globalBuffer.asVector();
      int const numInputNeurons       = globalVector.size();
      for (int k = 0; k < numInputNeurons; k++) {
         if (globalVector[k] != correctValue) {
            outputStream->printf(
                  "Time %f, input layer neuron %d is %f, instead of the expected %f.\n",
                  timevalue,
                  k,
                  (double)globalVector[k],
                  (double)correctValue);
            failed = true;
         }
      }
   }
   return failed;
}

bool HyPerConnCheckpointerTestProbe::verifyConnection(
      PV::HyPerConn *connection,
      float correctValue,
      double timevalue) {
   bool failed = false;

   if (parent->getCommunicator()->commRank() == 0) {
      float observedWeightValue = connection->get_wDataStart(0)[0];
      if (observedWeightValue != correctValue) {
         outputStream->printf(
               "Time %f, weight is %f, instead of the expected %f.\n",
               timevalue,
               (double)observedWeightValue,
               (double)correctValue);
         failed = true;
      }
   }
   return failed;
}
