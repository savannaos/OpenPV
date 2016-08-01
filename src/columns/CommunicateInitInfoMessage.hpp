/*
 * CommunicateInitInfoMessage.hpp
 *
 *  Created on: Jul 31, 2016
 *      Author: pschultz
 */

#ifndef COMMUNICATEINITINFOMESSAGE_HPP_
#define COMMUNICATEINITINFOMESSAGE_HPP_

#include <observerpattern/ObserverTable.hpp>
#include "columns/Messages.hpp"

namespace PV {

class CommunicateInitInfoMessage : public BaseMessage {
public:
   CommunicateInitInfoMessage(ObserverTable const& table) {
      setMessageType("CommunicateInitInfo");
      mTable = &table;
   }
   ObserverTable const * mTable;
}; // end class CommunicateInitInfoMessage

}  // end namespace PV

#endif /* COMMUNICATEINITINFOMESSAGE_HPP_ */
