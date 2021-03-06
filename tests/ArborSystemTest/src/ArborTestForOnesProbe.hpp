/*
 * ArborTestForOnesProbe.hpp
 *
 *  Created on: Sep 6, 2011
 *      Author: kpeterson
 */

#ifndef ArborTestForOnesProbe_HPP_
#define ArborTestForOnesProbe_HPP_

#include "probes/StatsProbe.hpp"

namespace PV {

class ArborTestForOnesProbe : public PV::StatsProbe {
  public:
   ArborTestForOnesProbe(const char *probeName, HyPerCol *hc);
   virtual ~ArborTestForOnesProbe();

   virtual int outputState(double timed);

  protected:
   int initArborTestForOnesProbe(const char *probeName, HyPerCol *hc);

  private:
   int initArborTestForOnesProbe_base();
};

} /* namespace PV */
#endif /* ArborTestForOnesProbe_HPP_ */
