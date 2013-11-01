/*
 * Movie.cpp
 *
 *  Created on: Sep 25, 2009
 *      Author: travel
 */

#include "Movie.hpp"
#include "../io/imageio.hpp"
#include "../include/default_params.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
//#include <iostream>

namespace PV {

Movie::Movie() {
   initialize_base();
}

Movie::Movie(const char * name, HyPerCol * hc, const char * fileOfFileNames) {
   initialize_base();
   initialize(name, hc, fileOfFileNames, DISPLAY_PERIOD);
}

Movie::Movie(const char * name, HyPerCol * hc, const char * fileOfFileNames, float defaultDisplayPeriod) {
   initialize_base();
   initialize(name, hc, fileOfFileNames, defaultDisplayPeriod);
}

int Movie::initialize_base() {
   movieOutputPath = NULL;
   skipFrameIndex = 0;
   echoFramePathnameFlag = false;
   filenamestream = NULL;
   displayPeriod = DISPLAY_PERIOD;
   readPvpFile = false;
   fileOfFileNames = NULL;
   frameNumber = 0;
   numFrames = 0;
   // newImageFlag = false;
   return PV_SUCCESS;
}

int Movie::checkpointRead(const char * cpDir, double * timef){
   Image::checkpointRead(cpDir, timef);

   if (this->useParamsImage) { //Sets nextDisplayTime = simulationtime (i.e. effectively restarting)
      nextDisplayTime += parent->simulationTime();
   }

   return PV_SUCCESS;
}

//
/*
 * Notes:
 * - writeImages, offsetX, offsetY are initialized by Image::initialize()
 */
int Movie::initialize(const char * name, HyPerCol * hc, const char * fileOfFileNames, float defaultDisplayPeriod) {
   displayPeriod = defaultDisplayPeriod; // Will be replaced with params value when setParams is called.
   int status = Image::initialize(name, hc, NULL);
   if (status != PV_SUCCESS) {
      fprintf(stderr, "Image::initialize failed on Movie layer \"%s\".  Exiting.\n", name);
      exit(PV_FAILURE);
   }

   if (fileOfFileNames != NULL) {
      this->fileOfFileNames = strdup(fileOfFileNames);
      if (this->fileOfFileNames==NULL) {
         fprintf(stderr, "Movie::initialize error in layer \"%s\": unable to copy fileOfFileNames: %s\n", name, strerror(errno));
      }
   }

   PVParams * params = hc->parameters();

   assert(!params->presentAndNotBeenRead(name, "displayPeriod"));
   nextDisplayTime = hc->simulationTime() + displayPeriod;

   assert(!params->presentAndNotBeenRead(name, "randomMovie")); // randomMovie should have been set in setParams
   if (randomMovie) return status; // Nothing else to be done until data buffer is allocated, in allocateDataStructures

   assert(!params->presentAndNotBeenRead(name, "readPvpFile")); // readPvpFile should have been set in setParams

   //If not pvp file, open fileOfFileNames 
   if( getParent()->icCommunicator()->commRank()==0 && !readPvpFile) {
      filenamestream = PV_fopen(fileOfFileNames, "r");
      if( filenamestream == NULL ) {
         fprintf(stderr, "Movie::initialize error opening \"%s\": %s\n", fileOfFileNames, strerror(errno));
         abort();
      }
   }

   if (!randomMovie) {
      if(readPvpFile){
         //Set filename as param
         filename = strdup(fileOfFileNames);
         assert(filename != NULL);
         //One indexed start_frame_index needs to be translated to zero indexed pvp file
         if (startFrameIndex <= 1){
            frameNumber = 0;
         }
         else{
            frameNumber = startFrameIndex - 1;
         }
         //Grab number of frames from header
         PV_Stream * pvstream = NULL;
         if (getParent()->icCommunicator()->commRank()==0) {
            pvstream = PV::PV_fopen(filename, "rb");
         }
         int numParams = NUM_PAR_BYTE_PARAMS;
         int params[numParams];
         pvp_read_header(pvstream, getParent()->icCommunicator(), params, &numParams);
         PV::PV_fclose(pvstream); pvstream = NULL;
         if(numParams != NUM_PAR_BYTE_PARAMS || params[INDEX_FILE_TYPE] != PVP_NONSPIKING_ACT_FILE_TYPE) {
            fprintf(stderr, "Movie layer \"%s\" error: file \"%s\" is not a nonspiking-activity pvp file.\n", name, filename);
            abort();
         }
         numFrames = params[INDEX_NBANDS];
      }
      else{
         // echoFramePathnameFlag = params->value(name,"echoFramePathnameFlag", false);
         filename = strdup(getNextFileName(startFrameIndex));
         assert(filename != NULL);
      }
   }

   // getImageInfo/constrainOffsets/readImage calls moved to Movie::allocateDataStructures

   // set output path for movie frames
   if(writeImages){
      // if ( params->stringPresent(name, "movieOutputPath") ) {
      //    movieOutputPath = strdup(params->stringValue(name, "movieOutputPath"));
      //    assert(movieOutputPath != NULL);
      // }
      // else {
      //    movieOutputPath = strdup( hc->getOutputPath());
      //    assert(movieOutputPath != NULL);
      //    printf("movieOutputPath is not specified in params file.\n"
      //          "movieOutputPath set to default \"%s\"\n",movieOutputPath);
      // }
      status = parent->ensureDirExists(movieOutputPath);
   }

   return PV_SUCCESS;
}

int Movie::setParams(PVParams * params) {
   int status = Image::setParams(params);
   displayPeriod = params->value(name,"displayPeriod", displayPeriod);
   randomMovie = (int) params->value(name,"randomMovie",0);
   if (randomMovie) {
      randomMovieProb   = params->value(name,"randomMovieProb", 0.05);  // 100 Hz
   }
   else {
      readPvpFile = (bool)params->value(name, "readPvpFile", 0);
      if (!readPvpFile) {
         echoFramePathnameFlag = params->value(name,"echoFramePathnameFlag", false);
      }
      startFrameIndex = params->value(name,"start_frame_index", 0);
      skipFrameIndex = params->value(name,"skip_frame_index", 0);
      autoResizeFlag = (bool)params->value(name,"autoResizeFlag",false);
   }
   assert(!params->presentAndNotBeenRead(name, "writeImages"));
   if(writeImages){
      if ( params->stringPresent(name, "movieOutputPath") ) {
         movieOutputPath = strdup(params->stringValue(name, "movieOutputPath"));
         assert(movieOutputPath != NULL);
      }
      else {
         movieOutputPath = strdup( parent->getOutputPath());
         assert(movieOutputPath != NULL);
         printf("movieOutputPath is not specified in params file.\n"
               "movieOutputPath set to default \"%s\"\n",movieOutputPath);
      }
   }
   return status;
}

Movie::~Movie()
{
   if (imageData != NULL) {
      delete imageData;
      imageData = NULL;
   }
   if (getParent()->icCommunicator()->commRank()==0 && filenamestream != NULL && filenamestream->isfile) {
      PV_fclose(filenamestream);
   }
   free(fileOfFileNames); fileOfFileNames = NULL;
}

int Movie::allocateDataStructures() {
   int status = Image::allocateDataStructures();

   if (!randomMovie) {
      assert(!parent->parameters()->presentAndNotBeenRead(name, "start_frame_index"));
      assert(!parent->parameters()->presentAndNotBeenRead(name, "skip_frame_index"));
      // skip to start_frame_index if provided
      // int start_frame_index = params->value(name,"start_frame_index", 0);
      // skipFrameIndex = params->value(name,"skip_frame_index", 0);

      // get size info from image so that data buffer can be allocated
      GDALColorInterp * colorbandtypes = NULL;
      status = getImageInfo(filename, parent->icCommunicator(), &imageLoc, &colorbandtypes);
      if(status != 0) {
         fprintf(stderr, "Movie: Unable to get image info for \"%s\"\n", filename);
         abort();
      }

      assert(!parent->parameters()->presentAndNotBeenRead(name, "autoResizeFlag"));
      if (!autoResizeFlag){
         constrainOffsets();  // ensure that offsets keep loc within image bounds
      }

      status = readImage(filename, getOffsetX(), getOffsetY(), colorbandtypes);
      assert(status == PV_SUCCESS);

      free(colorbandtypes); colorbandtypes = NULL;
   }
   else {
      status = randomFrame();
   }

   // exchange border information
   exchange();

   // newImageFlag = true;
   return status;
}

pvdata_t * Movie::getImageBuffer()
{
   //   return imageData;
   return data;
}

PVLayerLoc Movie::getImageLoc()
{
   return imageLoc;
   //   return clayer->loc;
   // imageLoc contains size information of the image file being loaded;
   // clayer->loc contains size information of the layer, which may
   // be smaller than the whole image.  To get information on the layer, use
   // getLayerLoc().  --pete 2011-07-10
}

int Movie::updateState(double time, double dt)
{
   updateImage(time, dt);
   return 0;
}

/**
 * - Update the image buffers
 * - If the time is a multiple of biasChangetime then the position of the bias (biasX, biasY) changes.
 * - With probability persistenceProb the offset position (offsetX, offsetY) remains unchanged.
 * - Otherwise, with probability (1-persistenceProb) the offset position performs a random walk
 *   around the bias position (biasX, biasY).
 *
 * - If the time is a multiple of displayPeriod then load the next image.
 * - If nf=1 then the image is converted to grayscale during the call to read(filename, offsetX, offsetY).
 *   If nf>1 then the image is loaded with color information preserved.
 * - Return true if buffers have changed
 */
bool Movie::updateImage(double time, double dt)
{
   InterColComm * icComm = getParent()->icCommunicator();
   if(randomMovie){
      randomFrame();
      lastUpdateTime = time;
   } else {
      bool needNewImage = false;
      while (time >= nextDisplayTime) {
         needNewImage = true;
         if (readPvpFile){
            //If set to 0 or 1, normal frame
            if (skipFrameIndex <= 1){
               frameNumber += 1;
            }
            //Otherwise, skip based on skipFrameIndex
            else{
               frameNumber += skipFrameIndex;
            }
            //Loop when frame number reaches numFrames
            if (frameNumber >= numFrames){
               if( icComm->commRank()==0 ) {
                  fprintf(stderr, "Movie %s: EOF reached, rewinding file \"%s\"\n", name, filename);
               }
               frameNumber = 0;
            }
         }
         else{
            if (filename != NULL) free(filename);
            filename = strdup(getNextFileName(skipFrameIndex));
            assert(filename != NULL);
         }
         nextDisplayTime += displayPeriod;

         if(writePosition && parent->icCommunicator()->commRank()==0){
            fprintf(fp_pos->fp,"%f %s: \n",time,filename);
         }
         lastUpdateTime = time;
      } // time >= nextDisplayTime

      if( jitterFlag ) {
         bool jittered = jitter();
         needNewImage |= jittered;
      } // jitterFlag

      if( needNewImage ){
         GDALColorInterp * colorbandtypes = NULL;
         int status = getImageInfo(filename, parent->icCommunicator(), &imageLoc, &colorbandtypes);
         if( status != PV_SUCCESS ) {
            fprintf(stderr, "Movie %s: Error getting image info \"%s\"\n", name, filename);
            abort();
         }
         //Set frame number (member variable in Image)
         if( status == PV_SUCCESS ) status = readImage(filename, getOffsetX(), getOffsetY(), colorbandtypes);
         free(colorbandtypes); colorbandtypes = NULL;
         if( status != PV_SUCCESS ) {
            fprintf(stderr, "Movie %s: Error reading file \"%s\"\n", name, filename);
            abort();
         }
         // newImageFlag = true;
         lastUpdateTime = parent->simulationTime();
      }
      // else{
      //    if (time>0.0) newImageFlag = false;
      // }
   } // randomMovie

   // exchange border information
   exchange();

   return true;
}

/**
 * When we play a random frame - in order to perform a reverse correlation analysis -
 * we call writeActivitySparse(time) in order to write the "activity" in the image.
 *
 */
int Movie::outputState(double timed, bool last)
{
   if (writeImages) {
      char basicFilename[PV_PATH_MAX + 1];
      snprintf(basicFilename, PV_PATH_MAX, "%s/Movie_%.2f.%s", movieOutputPath, timed, writeImagesExtension);
      write(basicFilename);
   }

   int status = PV_SUCCESS;
   if (randomMovie != 0) {
      status = writeActivitySparse(timed, false/*includeValues*/);
   }
   else {
      status = HyPerLayer::outputState(timed, last);
   }

   return status;
}

int Movie::copyReducedImagePortion()
{
   const PVLayerLoc * loc = getLayerLoc();

   const int nx = loc->nx;
   const int ny = loc->ny;

   const int nx0 = imageLoc.nx;
   const int ny0 = imageLoc.ny;

   assert(nx0 <= nx);
   assert(ny0 <= ny);

   const int i0 = nx/2 - nx0/2;
   const int j0 = ny/2 - ny0/2;

   int ii = 0;
   for (int j = j0; j < j0+ny0; j++) {
      for (int i = i0; i < i0+nx0; i++) {
         imageData[ii++] = data[i+nx*j];
      }
   }

   return 0;
}

/**
 * This creates a random image patch (frame) that is used to perform a reverse correlation analysis
 * as the input signal propagates up the visual system's hierarchy.
 * NOTE: Check Image::toGrayScale() method which was the inspiration for this routine
 */
int Movie::randomFrame()
{
   assert(randomMovie); // randomMovieProb was set only if randomMovie is true
   for (int kex = 0; kex < clayer->numExtended; kex++) {
      double p = randState->uniformRandom();
      data[kex] = (p < randomMovieProb) ? 1: 0;
   }
   return 0;
}

// skip n_skip lines before reading next frame
const char * Movie::getNextFileName(int n_skip)
{
   for (int i_skip = 0; i_skip < n_skip-1; i_skip++){
      getNextFileName();
   }
   return getNextFileName();
}


const char * Movie::getNextFileName()
{
   InterColComm * icComm = getParent()->icCommunicator();
   if( icComm->commRank()==0 ) {
      int c;
      size_t maxlen = PV_PATH_MAX;

      //TODO: add recovery procedure to handle case where access to file is temporarily unavailable
      // use stat to verify status of filepointer, keep running tally of current frame index so that file can be reopened and advanced to current frame


      // if at end of file (EOF), rewind
      bool lineisblank = true;
      while(lineisblank) {
         if ((c = fgetc(filenamestream->fp)) == EOF) {
            PV_fseek(filenamestream, 0L, SEEK_SET);
            fprintf(stderr, "Movie %s: EOF reached, rewinding file \"%s\"\n", name, filename);
         }
         else {
            ungetc(c, filenamestream->fp);
         }

         char * path = fgets(inputfile, maxlen, filenamestream->fp);
         if (path != NULL) {
            filenamestream->filepos += strlen(path);
            path[PV_PATH_MAX-1] = '\0';
            size_t len = strlen(path);
            if (len > 0) {
               if (path[len-1] == '\n') {
                  path[len-1] = '\0';
                  len--;
               }
            }
            for (int j=0; j<len; j++) {
               if (!isblank(c)) {
                  lineisblank = false;
                  break;
               }
            }
         }
      }
      if (echoFramePathnameFlag){
         fprintf(stderr, "%s\n", inputfile);
      }
   }
#ifdef PV_USE_MPI
   MPI_Bcast(inputfile, PV_PATH_MAX, MPI_CHAR, 0, icComm->communicator());
#endif // PV_USE_MPI
   return inputfile;
}

// bool Movie::getNewImageFlag(){
//    return newImageFlag;
// }

const char * Movie::getCurrentImage(){
   return inputfile;
}

}
