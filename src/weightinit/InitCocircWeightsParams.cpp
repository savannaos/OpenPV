/*
 * InitCocircWeightsParams.cpp
 *
 *  Created on: Aug 10, 2011
 *      Author: kpeterson
 */

#include "InitCocircWeightsParams.hpp"

namespace PV {

InitCocircWeightsParams::InitCocircWeightsParams() { initialize_base(); }
InitCocircWeightsParams::InitCocircWeightsParams(const char *name, HyPerCol *hc)
      : InitGauss2DWeightsParams() {
   initialize_base();
   initialize(name, hc);
}

InitCocircWeightsParams::~InitCocircWeightsParams() {}

int InitCocircWeightsParams::initialize_base() {

   aspect   = 1.0f; // circular (not line oriented)
   sigma    = 0.8f;
   rMax     = 1.4f;
   strength = 1.0f;
   r2Max    = rMax * rMax;

   numFlanks = 1;
   shift     = 0.0f;
   setRotate(0.0f); // rotate so that axis isn't aligned
   setThetaMax(1.0f); // max orientation in units of PI

   sigma_cocirc = PI / 2.0f;

   sigma_kurve = 1.0f; // fraction of delta_radius_curvature

   setDeltaThetaMax(PI / 2.0f);

   cocirc_self = (pre != post);

   // from pv_common.h
   // // DK (1.0/(6*(NK-1)))   /*1/(sqrt(DX*DX+DY*DY)*(NK-1))*/         //  change in curvature
   delta_radius_curvature = 1.0f; // 1 = minimum radius of curvature

   // why are these hard coded in!!!:
   min_weight     = 0.0f; // read in as param
   POS_KURVE_FLAG = false; //  handle pos and neg curvature separately
   SADDLE_FLAG    = false; // handle saddle points separately

   return 1;
}

int InitCocircWeightsParams::initialize(const char *name, HyPerCol *hc) {
   InitGauss2DWeightsParams::initialize(name, hc);

   PVParams *params = parent->parameters();
   int status       = PV_SUCCESS;

   return status;
}

int InitCocircWeightsParams::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = InitGauss2DWeightsParams::ioParamsFillGroup(ioFlag);
   ioParam_sigmaCocirc(ioFlag);
   ioParam_sigmaKurve(ioFlag);
   ioParam_cocircSelf(ioFlag);
   ioParam_deltaRadiusCurvature(ioFlag);
   return status;
}

void InitCocircWeightsParams::ioParam_sigmaCocirc(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(ioFlag, name, "sigmaCocirc", &sigma_cocirc, sigma_cocirc);
}

void InitCocircWeightsParams::ioParam_sigmaKurve(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(ioFlag, name, "sigmaKurve", &sigma_kurve, sigma_kurve);
}

void InitCocircWeightsParams::ioParam_cocircSelf(enum ParamsIOFlag ioFlag) {
   parent->parameters()->ioParamValue(ioFlag, name, "cocircSelf", &cocirc_self, cocirc_self);
}

void InitCocircWeightsParams::ioParam_deltaRadiusCurvature(enum ParamsIOFlag ioFlag) {
   // from pv_common.h
   // // DK (1.0/(6*(NK-1)))   /*1/(sqrt(DX*DX+DY*DY)*(NK-1))*/         //  change in curvature
   parent->parameters()->ioParamValue(
         ioFlag, name, "deltaRadiusCurvature", &delta_radius_curvature, delta_radius_curvature);
}

void InitCocircWeightsParams::ioParam_numOrientationsPre(enum ParamsIOFlag ioFlag) {
   assert(post);
   const char *paramname = "numOrientationsPre";
   parent->parameters()->ioParamValue(
         ioFlag, name, paramname, &numOrientationsPre, pre->getLayerLoc()->nf);
}

void InitCocircWeightsParams::ioParam_numOrientationsPost(enum ParamsIOFlag ioFlag) {
   assert(post);
   const char *paramname = "numOrientationsPost";
   parent->parameters()->ioParamValue(
         ioFlag, name, paramname, &numOrientationsPost, post->getLayerLoc()->nf);
}

void InitCocircWeightsParams::calcOtherParams(int patchIndex) {
   this->getcheckdimensionsandstrides();

   const int kfPre_tmp = this->kernelIndexCalculations(patchIndex);
   nKurvePre           = pre->getLayerLoc()->nf / getNoPre();
   nKurvePost          = post->getLayerLoc()->nf / getNoPost();
   this->calculateThetas(kfPre_tmp, patchIndex);
}

float InitCocircWeightsParams::calcKurvePostAndSigmaKurvePost(int kfPost) {
   int iKvPost       = kfPost % nKurvePost;
   float radKurvPost = calcKurveAndSigmaKurve(
         iKvPost, nKurvePost, sigma_kurve_post, kurvePost, iPosKurvePost, iSaddlePost);
   sigma_kurve_post2 = 2 * sigma_kurve_post * sigma_kurve_post;
   return radKurvPost;
}

float InitCocircWeightsParams::calcKurveAndSigmaKurve(
      int kf,
      int &nKurve,
      float &sigma_kurve_temp,
      float &kurve_tmp,
      bool &iPosKurve,
      bool &iSaddle) {
   int iKv          = kf % nKurve;
   iPosKurve        = false;
   iSaddle          = false;
   float radKurv    = delta_radius_curvature + iKv * delta_radius_curvature;
   sigma_kurve_temp = sigma_kurve * radKurv;

   kurve_tmp = (radKurv != 0.0f) ? 1 / radKurv : 1.0f;

   int iKvPostAdj = iKv;
   if (POS_KURVE_FLAG) {
      assert(nKurve >= 2);
      iPosKurve = iKv >= (int)(nKurve / 2);
      if (SADDLE_FLAG) {
         assert(nKurve >= 4);
         iSaddle    = (iKv % 2 == 0) ? 0 : 1;
         iKvPostAdj = ((iKv % (nKurve / 2)) / 2);
      }
      else { // SADDLE_FLAG
         iKvPostAdj = (iKv % (nKurve / 2));
      }
   } // POS_KURVE_FLAG
   radKurv   = delta_radius_curvature + iKvPostAdj * delta_radius_curvature;
   kurve_tmp = (radKurv != 0.0f) ? 1 / radKurv : 1.0f;
   return radKurv;
}

bool InitCocircWeightsParams::checkSameLoc(int kfPost) {
   const float sigma_cocirc2 = 2 * sigma_cocirc * sigma_cocirc;
   bool sameLoc              = (getFPre() == kfPost);
   if ((!sameLoc) || (cocirc_self)) {
      gCocirc = sigma_cocirc > 0 ? expf(-getDeltaTheta() * getDeltaTheta() / sigma_cocirc2)
                                 : expf(-getDeltaTheta() * getDeltaTheta() / sigma_cocirc2) - 1.0f;
      if ((nKurvePre > 1) && (nKurvePost > 1)) {
         gKurvePre =
               expf(-(kurvePre - kurvePost) * (kurvePre - kurvePost)
                    / (sigma_kurve_pre2 + sigma_kurve_post2));
      }
   }
   else { // sameLoc && !cocircSelf
      gCocirc = 0.0f;
      return true;
   }
   return false;
}

bool InitCocircWeightsParams::checkFlags(float dyP_shift, float dxP) {
   if (POS_KURVE_FLAG) {
      if (SADDLE_FLAG) {
         if ((iPosKurvePre) && !(iSaddlePre) && (dyP_shift < 0)) {
            return true;
         }
         if (!(iPosKurvePre) && !(iSaddlePre) && (dyP_shift > 0)) {
            return true;
         }
         if ((iPosKurvePre) && (iSaddlePre)
             && (((dyP_shift > 0) && (dxP < 0)) || ((dyP_shift > 0) && (dxP < 0)))) {
            return true;
         }
         if (!(iPosKurvePre) && (iSaddlePre)
             && (((dyP_shift > 0) && (dxP > 0)) || ((dyP_shift < 0) && (dxP < 0)))) {
            return true;
         }
      }
      else { // SADDLE_FLAG
         if ((iPosKurvePre) && (dyP_shift < 0)) {
            return true;
         }
         if (!(iPosKurvePre) && (dyP_shift > 0)) {
            return true;
         }
      }
   } // POS_KURVE_FLAG
   return false;
}

void InitCocircWeightsParams::updateCocircNChord(
      float thPost,
      float dyP_shift,
      float dxP,
      float cocircKurve_shift,
      float d2_shift) {

   const float thetaPre      = getthPre();
   const int noPre           = getNoPre();
   const int noPost          = getNoPost();
   const float sigma_cocirc2 = 2 * getSigma_cocirc() * getSigma_cocirc();
   const int nKurvePre       = (int)getnKurvePre();

   float atanx2_shift = thetaPre + 2.0f * atan2f(dyP_shift, dxP); // preferred angle (rad)
   atanx2_shift += 2.0f * PI;
   atanx2_shift    = fmodf(atanx2_shift, PI);
   atanx2_shift    = fabsf(atanx2_shift - thPost);
   float chi_shift = atanx2_shift;
   if (chi_shift >= PI / 2.0f) {
      chi_shift = PI - chi_shift;
   }
   if (noPre > 1 && noPost > 1) {
      gCocirc = sigma_cocirc2 > 0 ? expf(-chi_shift * chi_shift / sigma_cocirc2)
                                  : expf(-chi_shift * chi_shift / sigma_cocirc2) - 1.0f;
   }
}

void InitCocircWeightsParams::updategKurvePreNgKurvePost(float cocircKurve_shift) {

   const float sigma_cocirc2     = 2 * getSigma_cocirc() * getSigma_cocirc();
   const float sigma_kurve_pre2  = getSigma_kurve_pre2();
   const float sigma_kurve_post2 = getSigma_kurve_post2();

   gKurvePre = (nKurvePre > 1)
                     ? expf(-powf((cocircKurve_shift - fabsf(kurvePre)), 2) / sigma_kurve_pre2)
                     : 1.0f;
   gKurvePost = ((nKurvePre > 1) && (nKurvePost > 1) && (sigma_cocirc2 > 0))
                      ? expf(-powf((cocircKurve_shift - fabsf(kurvePost)), 2) / sigma_kurve_post2)
                      : 1.0f;
}

void InitCocircWeightsParams::initializeDistChordCocircKurvePreAndKurvePost() {
   gDist      = 0.0f;
   gCocirc    = 1.0f;
   gKurvePre  = 1.0f;
   gKurvePost = 1.0f;
}

float InitCocircWeightsParams::calculateWeight() {
   return gDist * gKurvePre * gKurvePost * gCocirc;
}

void InitCocircWeightsParams::addToGDist(float inc) { gDist += inc; }

} /* namespace PV */
