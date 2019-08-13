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

#include "sim-helper.hpp"
#include "../helper/ndn-stack-helper.hpp"
#include "../helper/ndn-fib-helper.hpp"
#include "../helper/ndn-strategy-choice-helper.hpp"
#include "../utils/topology/annotated-topology-reader.hpp"
#include "../helper/ndn-global-routing-helper.hpp"
#include "../utils/tracers/l2-rate-tracer.hpp"
#include "../utils/tracers/ndn-app-delay-tracer.hpp"
#include "../utils/tracers/ndn-l3-rate-tracer.hpp"
#include "../helper/ndn-app-helper.hpp"
#include "../NFD/daemon/fw/pcon-strategy.hpp"
#include "../../core/model/names.h"
#include "../../core/model/config.h"
#include "../../network/helper/node-container.h"

namespace ns3 {

int
main(int argc, char* argv[])
{
  const std::string SCEN_NAME = SimHelper::getEnvVariableStr("SCEN_NAME", "PCON_4.1");
  const int RUNTIME = SimHelper::getEnvVariable("RUNTIME", 30);
  const uint32_t QUEUE_SIZE_PKTS = SimHelper::getEnvVariable("QUEUE_SIZE_PKTS", 1000);

  // Set to "FIFO", "CODEL", or "PCON".
  const std::string QUEUE_TYPE = SimHelper::getEnvVariableStr("QUEUE_TYPE", "PCON");

  // You may want to edit enviroment variable "CHANGE_PER_MARK",
  // see pcon-strategy.cpp for the detail.

  int MIN_RTO = SimHelper::getEnvVariable("MIN_RTO", 10);

  // Read Topology
  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/topo-pcon_scen4.2.txt");
  topologyReader.Read();

  NodeContainer consumers;
  NodeContainer routers;
  NodeContainer producers;
  NodeContainer allNodes = topologyReader.GetNodes();

  SimHelper::fillNodeContainer(consumers, "consumer");
  SimHelper::fillNodeContainer(routers, "mrouter");
  SimHelper::fillNodeContainer(producers, "producer");

  std::cout << "Scen: " << SCEN_NAME << ", queue type: " << QUEUE_TYPE << ", runtime: "
      << RUNTIME << "s, nodes: " << allNodes.size() << "\n";

  for (auto node : allNodes) {
    SimHelper::setNodeQueue(node, QUEUE_SIZE_PKTS, QUEUE_TYPE);
  }
  if (QUEUE_TYPE == "PCON") {
    MIN_RTO = 300;
  }

  // Install NDN stack on all nodes
  ndn::StackHelper stackHelper;
  stackHelper.SetDefaultRoutes(false);

  // Set CS Size
  stackHelper.setCsSize(200000);
  stackHelper.InstallAll();

  // Setting FIBs manually to avoid routing loops:
  for (unsigned i = 1; i <= consumers.size(); i++) {
    ndn::FibHelper::AddRoute("consumer" + std::to_string(i), "/prefix/A", "mrouter1",
        100);
  }
  ndn::FibHelper::AddRoute("mrouter1", "/prefix/A", "mrouter2", 100);
  ndn::FibHelper::AddRoute("mrouter1", "/prefix/A", "producer4", 100);

  ndn::FibHelper::AddRoute("mrouter2", "/prefix/A", "producer1", 100);
  ndn::FibHelper::AddRoute("mrouter2", "/prefix/A", "producer2", 100);
  ndn::FibHelper::AddRoute("mrouter2", "/prefix/A", "producer3", 100);

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll<nfd::fw::PconStrategy>("/");

  // Installing applications
  ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerCC");
  consumerHelper1.SetPrefix("/prefix/A");
  consumerHelper1.SetAttribute("MinRto", UintegerValue(MIN_RTO));
  consumerHelper1.SetAttribute("Size", StringValue("1000M"));

  // Each consumer node retrieves different data:
  for (uint32_t i = 0; i < consumers.size(); i++) {
        consumerHelper1.SetPrefix("/prefix/A/" + std::to_string(i));
        consumerHelper1.SetAttribute("StartTime", StringValue(std::to_string(i) + "s"));
        consumerHelper1.Install(consumers[i]);
    }

  // Producer
  ndn::AppHelper producerHelper1("ns3::ndn::Producer");
  producerHelper1.SetPrefix("/prefix/A");
  producerHelper1.SetAttribute("PayloadSize", UintegerValue(1024));
  for (ns3::Ptr<ns3::Node> n : producers) {
    producerHelper1.Install(n);
  }

  // setting default parameters for PointToPoint links and channels
  std::string folder = "results/";
  std::string ratesFile = folder + "rates.txt";
  std::string dropFile = folder + "drop.txt";
  std::string delayFile = folder + "delay.txt";

  L2RateTracer::InstallAll(dropFile, Seconds(0.10));
  ndn::L3RateTracer::InstallAll(ratesFile, Seconds(0.10));
  ndn::AppDelayTracer::InstallAll(delayFile);

  Simulator::Stop(Seconds(RUNTIME));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "\nSimulation finished!\n";

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
