/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2016 Klaus Schneider, The University of Arizona
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Klaus Schneider <klaus@cs.arizona.edu>
 */

#ifndef NFD_DAEMON_FW_STRATEGY_HELPER_HPP
#define NFD_DAEMON_FW_STRATEGY_HELPER_HPP

//#include "../../common.hpp"
#include "../../../utils/ndn-ns3-packet-tag.hpp"
#include "../../../utils/ndn-ns3-cc-tag.hpp"
#include "../../../../core/model/ptr.h"
#include "../../../../internet/model/codel-queue2.h"
#include "../../../../point-to-point/model/point-to-point-net-device.h"
#include "../../../model/ndn-net-device-face.hpp"
#include "mt-forwarding-info.hpp"

namespace nfd {
namespace fw {

using ndn::Name; 

class StrHelper
{
public:
  static const uint DOWN_FACE_METRIC = 7;

public:

  static void
  reduceFwPerc(shared_ptr<MtForwardingInfo> forwInfo,
      const FaceId reducedFaceId,
      const double change)
  {

    if (forwInfo->getFaceCount() == 1) {
      std::cout << "Trying to update fw perc of single face!\n";
      return;
    }

    // Reduction is at most the current forwarding percentage of the face that is reduced.
    double changeRate = 0 - std::min(change, forwInfo->getforwPerc(reducedFaceId));

    // Decrease fw percentage of the given face:
    forwInfo->increaseforwPerc(reducedFaceId, changeRate);
    double sumFWPerc = 0;
    sumFWPerc += forwInfo->getforwPerc(reducedFaceId);
    const auto forwMap = forwInfo->getForwPercMap();

//		std::cout << "\n";
    for (auto f : forwMap) {
      auto tempChangeRate = changeRate;
      auto &faceId = f.first;
      if (faceId == reducedFaceId) { // Do nothing. Percentage has already been added.
      }
      else {
        // Increase forwarding percentage of all other faces by and equal amount.
        tempChangeRate = std::abs(changeRate / (double) (forwMap.size() - 1));
        forwInfo->increaseforwPerc((faceId), tempChangeRate);
        sumFWPerc += forwInfo->getforwPerc(faceId);
      }
    }

    if (sumFWPerc < 0.999 || sumFWPerc > 1.001) {
      std::cout << StrHelper::getTime() << "ERROR! Sum of fw perc out of range: " << sumFWPerc
          << "\n";
    }
  }

  /** \brief determines whether a NextHop is eligible
   *  \param currentDownstream incoming FaceId of current Interest
   *  \param wantUnused if true, NextHop must not have unexpired OutRecord
   *  \param now time::steady_clock::now(), ignored if !wantUnused
   */
  static bool
  predicate_NextHop_eligible(const shared_ptr<pit::Entry>& pitEntry,
      const fib::NextHop& nexthop,
      FaceId currentDownstream, 
      bool wantUnused = false,
      ndn::time::steady_clock::TimePoint now = ndn::time::steady_clock::TimePoint::min())
  // TODO Implement congestion action.
  {
    shared_ptr<Face> upstream = nexthop.getFace();

    // upstream is current downstream
    if (upstream->getId() == currentDownstream)
      return false;
    // forwarding would violate scope
    if (pitEntry->violatesScope(*upstream))
      return false;
    if (upstream->getMetric() == DOWN_FACE_METRIC) {
      return false;
    }

    if (wantUnused) {
      // NextHop must not have unexpired OutRecord
      pit::OutRecordCollection::const_iterator outRecord = pitEntry->getOutRecord(*upstream);
      if (outRecord != pitEntry->getOutRecords().end() && outRecord->getExpiry() > now) {
        return false;
      }
    }

    return true;
  }

  static int8_t
  getNackType(const ::ndn::TagHost& packet)
  {
    auto tag = getCCTag(packet);
    if (tag != nullptr) {
      return tag->getNackType();
    }
    else {
      // NACK_TYPE_NONE
      return -1;
    }
  }

  static int8_t
  getCongMark(const ::ndn::TagHost& packet)
  {
    auto tag = getCCTag(packet);
    if (tag != nullptr) {
      return tag->getCongMark();
    }
    else {
      return 0;
    }
  }


  static shared_ptr<ns3::ndn::Ns3CCTag>
  getCCTag(const ::ndn::TagHost& packet)
  {
    auto tag = packet.getTag<ns3::ndn::Ns3PacketTag>();
    if (tag != nullptr) {
      ns3::Ptr<const ns3::Packet> pkt = tag->getPacket();
      ns3::ndn::Ns3CCTag tempTag;  
      bool hasTag = pkt->PeekPacketTag(tempTag);
//      std::cout << "Has CC Tag? " << hasTag << "\n";
      shared_ptr<ns3::ndn::Ns3CCTag> ns3ccTag = make_shared<ns3::ndn::Ns3CCTag>();
      if (hasTag) {
        auto it = pkt->GetPacketTagIterator();
        int i = 0;
        while (it.HasNext()) {
          i++;
          auto n = it.Next();
          if (n.GetTypeId() == ns3::ndn::Ns3CCTag::GetTypeId()) {
            n.GetTag(*ns3ccTag);
            break;
          }
        }
        return ns3ccTag;
      }
      else {
        return nullptr;
      }
    }
    return nullptr;
  }

  static shared_ptr<MtForwardingInfo>
  getPrefixMeasurements(const fib::Entry& fibEntry, MeasurementsAccessor& measurements)
  {
    shared_ptr<measurements::Entry> me = measurements.get(fibEntry);
    if (me == nullptr) {
      std::cout << "Didn't find measurement entry for name: " << fibEntry.getPrefix() << "\n";
      return nullptr;
    }
    return me->getStrategyInfo<MtForwardingInfo>();
  }

  static shared_ptr<MtForwardingInfo>
  addPrefixMeasurements(const fib::Entry& fibEntry, MeasurementsAccessor& measurements)
  {
    shared_ptr<measurements::Entry> me = measurements.get(fibEntry);
    return me->getOrCreateStrategyInfo<MtForwardingInfo>();
  }

  static std::tuple<Name, shared_ptr<MtForwardingInfo>>
  findPrefixMeasurementsLPM(const pit::Entry& pitEntry, MeasurementsAccessor& measurements)
  {
    
//    shared_ptr<measurements::Entry> me = measurements.get(name);
//    shared_ptr<measurements::Entry> me = measurements.findLongestPrefixMatch(name);
    shared_ptr<measurements::Entry> me = measurements.findLongestPrefixMatch(pitEntry);
    if (me == nullptr) {
      std::cout << "Name " << pitEntry.getName().toUri() << " not found!\n";
      return std::forward_as_tuple(Name(), nullptr); 
    }
    shared_ptr<MtForwardingInfo> mi = me->getStrategyInfo<MtForwardingInfo>();
    assert(mi != nullptr);
    return std::forward_as_tuple(me->getName(), mi);
  }

  static shared_ptr<MtForwardingInfo>
  addPrefixMeasurementsLPM(const Name& name, MeasurementsAccessor& measurements)
  {
    shared_ptr<measurements::Entry> me;
    // Save info one step up.
    me = measurements.get(name);
    // parent of Interest Name is not in this strategy, or Interest Name is empty
    return me->getOrCreateStrategyInfo<MtForwardingInfo>();
  }

  /**
   * Returns true if packet should be marked according to CoDel logic.
   */
  static bool
  isCongested(Forwarder& forwarder, int faceid)
  {
    auto codelQueue = getCodelQueue(forwarder, faceid);

    if (codelQueue != nullptr) {
      //        std::cout << "Got CodelQueue!\n";
      //        std::cout << "Codel dropping state: " << codelQueue->isInDroppingState() << "\n";
      bool shouildBeMarked = codelQueue->isOkToMark();
      return shouildBeMarked;
    }
    // Error
    return false;
  }

  /**
   * Returns true if the queue size is over thresholdPerc or the CoDel queuing delay is over thresholdDelayInMs
   */
  static bool
  isHighlyCongested(Forwarder& forwarder,
      int faceid,
      double thresholdPerc = 0.9,
      double thresholdDelayInMs = 1000)
  {
    auto codelQueue = getCodelQueue(forwarder, faceid);
    if (codelQueue != nullptr) {

      // Queue fuller than threshold, default: 90%! 
      bool queueAlmostFull = codelQueue->isQueueOverLimit(thresholdPerc);
      int64_t timeOverLimitNano = codelQueue->getTimeOverLimitInNS();
      double timeOverMS = (double) timeOverLimitNano / (1000 * 1000);

      // Set as highly congested if queuing delay over thresholdDelayInMs (default 1s) 
      // or queue over thresholdPerc:
      if (timeOverMS > thresholdDelayInMs || queueAlmostFull) {
//      std::cout << "StrHelper: Queue over high threshold!\n";
        return true;
      }
      else {
        return false;
      }
    }
    // Error
    return false;
  }

  static std::string
  getTime()
  {
    std::ostringstream oss;
    oss << ndn::time::steady_clock::now().time_since_epoch().count() / 1000000 << " ms";
    return oss.str();
  }

  // Print table: time, node, faceid, type, fwPerc
  static void 
  printFwPerc(shared_ptr<std::ostream> os,
      uint32_t nodeId,
      std::string prefix,
      FaceId faceId,
      std::string type,
      double fwPerc)
  {
    *os << ns3::Simulator::Now().ToDouble(ns3::Time::S) << "\t" << nodeId << "\t" << prefix << "\t"
        << faceId << "\t" << type << "\t" << fwPerc << "\n";
//		os->flush();
  }

  static double
  getDoubleEnvVariable(std::string name, double defaultValue)
  {
    std::string tmp = StrHelper::getEnvVar(name);
    if (!tmp.empty()) {
      return std::stod(tmp);
    }
    else {
      return defaultValue;
    }
  }

  static bool
  getEnvVariable(std::string name, bool defaultValue)
  {
    std::string tmp = StrHelper::getEnvVar(name);
    if (!tmp.empty()) {
      return (tmp == "TRUE" || tmp == "true");
    }
    else {
      return defaultValue;
    }
  }

  static int
  getEnvVariable(std::string name, int defaultValue)
  {
    std::string tmp = StrHelper::getEnvVar(name);
    if (!tmp.empty()) {
      return std::stoi(tmp);
    }
    else {
      return defaultValue;
    }
  }

private:

  static std::string
  getEnvVar(std::string const& key)
  {
    char const* val = getenv(key.c_str());
    return val == NULL ? std::string() : std::string(val);
  }

  static ns3::Ptr<ns3::CoDelQueue2>
  getCodelQueue(Forwarder& forwarder, int faceid)
  {
    shared_ptr<Face> face = forwarder.getFace(faceid);
    shared_ptr<ns3::ndn::NetDeviceFace> netDeviceFace = std::dynamic_pointer_cast<
        ns3::ndn::NetDeviceFace>(face);
    if (netDeviceFace == nullptr || netDeviceFace == 0) {
      // do nothing
    }
    else {
      ns3::Ptr<ns3::NetDevice> netdevice { netDeviceFace->GetNetDevice() };

      if (netdevice == nullptr) {
        std::cout << "Netdev nullptr!\n";
      }
 
      ns3::Ptr<ns3::PointToPointNetDevice> ptp = ns3::DynamicCast<ns3::PointToPointNetDevice>(
          netdevice);
 
      if (ptp == nullptr) {
        std::cout << "Not a PointToPointNetDevice\n";
      }
      else {
        auto q = ptp->GetQueue();  
        auto codelQueue = ns3::DynamicCast<ns3::CoDelQueue2>(q);
        return codelQueue;
      }
    }
    // Error
    return nullptr;
  }
  

  static bool
  getHighCongMark(const ::ndn::TagHost& packet)
  { 
    shared_ptr<ns3::ndn::Ns3CCTag> tag = getCCTag(packet);
    if (tag != nullptr) {
      return tag->getHighCongMark();
    }
    else {
      return false;
    }
  }  

  static void
  printFIB(Fib& fib)
  { 
    Fib::const_iterator it = fib.begin();
    while (it != fib.end()) {
      std::cout << it->getPrefix() << ": ";
      for (auto b : it->getNextHops()) {
        std::cout << b.getFace()->getLocalUri() << b.getFace()->getId() << ", local:" << ", ";
      }
      std::cout << "\n";
      it++;
    }
  }

  static void
  printFIBEntry(const shared_ptr<fib::Entry> entry)
  {
    for (auto f : entry->getNextHops()) {
      std::cout << f.getFace()->getLocalUri() << f.getFace()->getId() << ", ";
    }
    std::cout << "\n";
  } 

}
;

} // namespace fw
} // namespace nfd

#endif

