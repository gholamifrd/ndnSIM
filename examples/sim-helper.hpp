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

#ifndef PCON_SIM_HELPER_HPP
#define PCON_SIM_HELPER_HPP

#include "../../core/model/ptr.h"
#include "../../core/model/object.h"
#include "../../core/model/uinteger.h"
#include "../../network/model/node.h"
#include "../../network/utils/data-rate.h"
#include "../../network/utils/queue.h"
#include "../../point-to-point/model/point-to-point-net-device.h"
#include "../../internet/model/codel-queue2.h"
#include "../../internet/model/codel-queue.h"
#include "../../network/utils/drop-tail-queue.h"
#include "../../network/helper/node-container.h"
#include "../../core/model/names.h"
#include <assert.h>

namespace ns3 {

class SimHelper
{
public:

  /**
   * Set all outgoing links of a given node to use the CoDel queue.
   *
   * \param origCodel: If true, use standard dropping CoDel queue,
   *                   if false, use PCON marking CoDel queue.
   *
   * \param queueSize: Maximum queue size in packets.
   */
  static void
  setNodeQueue(Ptr<Node> node, uint32_t queueSize, std::string QUEUE_TYPE = "PCON")
  {
    for (unsigned i = 0; i < node->GetNDevices(); i++) {
      auto n = node->GetDevice(i);
      auto b = ns3::DynamicCast<ns3::PointToPointNetDevice>(n);

      if (QUEUE_TYPE == "FIFO") {
        ns3::Ptr<ns3::Queue> fifo = ns3::CreateObject<ns3::DropTailQueue>();
        fifo->SetAttribute("Mode", EnumValue(0));
        fifo->SetAttribute("MaxPackets", UintegerValue(queueSize));
        b->SetQueue(fifo);
      }
      else if (QUEUE_TYPE == "CODEL") {
        ns3::Ptr<ns3::Queue> codel = ns3::CreateObject<ns3::CoDelQueue>();
        codel->SetAttribute("Mode", EnumValue(0));
        codel->SetAttribute("MaxPackets", UintegerValue(queueSize));
        b->SetQueue(codel);
      }
      else if (QUEUE_TYPE == "PCON") {
        ns3::Ptr<ns3::Queue> codel2 = ns3::CreateObject<ns3::CoDelQueue2>();
        codel2->SetAttribute("Mode", EnumValue(0));
        codel2->SetAttribute("MaxPackets", UintegerValue(queueSize));
        b->SetQueue(codel2);
      }
      else {
        std::cout << "Wrong Queue Type!!\n";
        assert(false);
      }
    }
  }

  static void
  fillNodeContainer(NodeContainer& container, std::string nodeName)
  {
    int MAX_FACES = 1000;
    for (int i = 1; i <= MAX_FACES; i++) {
      auto cons = Names::Find<Node>(nodeName + std::to_string(i));
      if (cons != nullptr) {
        container.Add(cons);
      }
      else {
        break;
      }
    }

  }

  static void
  setLinkBW(Ptr<Node> node, double LINK_BANDWIDTH_IN_MBPS)
  {

    for (unsigned i = 0; i < node->GetNDevices(); i++) {
      auto n = node->GetDevice(i);
      auto dev = DynamicCast<ns3::PointToPointNetDevice>(n);
      DataRate rate = DataRate(LINK_BANDWIDTH_IN_MBPS * 1000 * 1000);
      dev->SetDataRate(rate);
    }
  }

  static std::string
  getEnvVar(std::string const& key)
  {
    char const* val = getenv(key.c_str());
    return val == NULL ? std::string() : std::string(val);
  }

  static std::string
  getEnvVariableStr(std::string name, std::string defaultValue)
  {
    std::string tmp = getEnvVar(name);
    if (!tmp.empty()) {
      return tmp;
    }
    else {
      return defaultValue;
    }
  }

  static int
  getEnvVariable(std::string name, int defaultValue)
  {
    std::string tmp = getEnvVar(name);
    if (!tmp.empty()) {
      return std::stoi(tmp);
    }
    else {
      return defaultValue;
    }
  }

  static double
  getEnvVariable(std::string name, double defaultValue)
  {
    std::string tmp = getEnvVar(name);
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
    std::string tmp = getEnvVar(name);
    if (!tmp.empty()) {
      return (tmp == "TRUE" || tmp == "true");
    }
    else {
      return defaultValue;
    }
  }

public:

}
;

} // namespace ns3

#endif

