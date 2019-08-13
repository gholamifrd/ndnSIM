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
  const std::string SCEN_NAME = SimHelper::getEnvVariableStr("SCEN_NAME", "PCON_3");
  const int RUNTIME = SimHelper::getEnvVariable("RUNTIME", 25);
  const uint32_t QUEUE_SIZE_PKTS = SimHelper::getEnvVariable("QUEUE_SIZE_PKTS", 1000);

  // Set to "FIFO", "CODEL", or "PCON".
  const std::string QUEUE_TYPE = SimHelper::getEnvVariableStr("QUEUE_TYPE", "PCON");

  const int CHANGE_TIME1 = SimHelper::getEnvVariable("CHANGE_TIME1", 5);
  const int CHANGE_TIME2 = SimHelper::getEnvVariable("CHANGE_TIME2", 15);
  const double CROSS_TRAFFIC_FACTOR = SimHelper::getEnvVariable("CROSS_TRAFFIC_FACTOR",
      0.95);
  int MIN_RTO = SimHelper::getEnvVariable("MIN_RTO", 10);

  // Read Topology
  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/topo-pcon_scen2.txt");
  topologyReader.Read();

  NodeContainer consumers;
  NodeContainer routers;
  NodeContainer producers;
  NodeContainer allNodes = topologyReader.GetNodes();

  SimHelper::fillNodeContainer(consumers, "consumer");
  SimHelper::fillNodeContainer(routers, "mrouter");
  SimHelper::fillNodeContainer(producers, "producer");

  std::cout << "Scen: " << SCEN_NAME << ", queue type: " << QUEUE_TYPE << ", runtime: "
      << RUNTIME << "s, nodes: " << allNodes.size() << "CROSS_TRAFFIC_FACTOR: "
      << CROSS_TRAFFIC_FACTOR << "\n";

  for (auto node : allNodes) {
    SimHelper::setNodeQueue(node, QUEUE_SIZE_PKTS, QUEUE_TYPE);
  }
  if (QUEUE_TYPE == "PCON") {
    MIN_RTO = 200;
  }

  // Install NDN stack on all nodes
  ndn::StackHelper stackHelper;
  stackHelper.SetDefaultRoutes(false);

  // Set CS Size
  stackHelper.setCsSize(200000);
  stackHelper.InstallAll();

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  for (ns3::Ptr<ns3::Node> n : producers) {
    ndnGlobalRoutingHelper.AddOrigins("/prefix/A", n);
  }
  ndnGlobalRoutingHelper.AddOrigins("/prefix/B", consumers[1]);
  ndn::GlobalRoutingHelper::CalculateRoutes();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll<nfd::fw::PconStrategy>("/");

  // Installing applications
  ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerCC");
  consumerHelper1.SetPrefix("/prefix/A");
  consumerHelper1.SetAttribute("MinRto", UintegerValue(MIN_RTO));
  consumerHelper1.SetAttribute("Size", StringValue("1000M"));
  consumerHelper1.Install(consumers[0]);

  ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerCbr");
  consumerHelper2.SetPrefix("/prefix/B");
  consumerHelper2.SetAttribute("StartTime",
      StringValue(std::to_string(CHANGE_TIME1) + "s"));
  consumerHelper2.SetAttribute("StopTime",
      StringValue(std::to_string(CHANGE_TIME2) + "s"));
  consumerHelper2.SetAttribute("Frequency",
      DoubleValue(124.15 * 10 * CROSS_TRAFFIC_FACTOR));

  // For other scenario: Install consumerHelper2 on consumer
  consumerHelper2.Install(producers[0]);

  // Producer
  const int ORIG_PAYLOAD_SIZE = 200;
  ndn::AppHelper producerHelper1("ns3::ndn::Producer");
  producerHelper1.SetPrefix("/prefix/A");
  producerHelper1.SetAttribute("PayloadSize", UintegerValue(ORIG_PAYLOAD_SIZE));

  ndn::AppHelper producerHelper2 { producerHelper1 };
  producerHelper2.SetPrefix("/prefix/B");
  producerHelper2.SetAttribute("PayloadSize", UintegerValue(1000));

  for (ns3::Ptr<ns3::Node> n : producers) {
    producerHelper1.Install(n);
  }
  producerHelper2.Install(consumers[1]);

  // setting default parameters for PointToPoint links and channels
  std::string folder = "results/";
  std::string ratesFile = folder + "rates.txt";
  std::string dropFile = folder + "drop.txt";
  std::string delayFile = folder + "delay.txt";

  L2RateTracer::InstallAll(dropFile, Seconds(0.05));
  ndn::L3RateTracer::InstallAll(ratesFile, Seconds(0.05));
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
