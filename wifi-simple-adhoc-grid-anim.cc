/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 */

//
// This program configures a grid (default 5x5) of nodes on an
// 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to node 1.
//
// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc-grid --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the ns-3 documentation.
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when distance increases beyond
// the default of 500m.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --distance=500"
// ./waf --run "wifi-simple-adhoc --distance=1000"
// ./waf --run "wifi-simple-adhoc --distance=1500"
//
// The source node and sink node can be changed like this:
//
// ./waf --run "wifi-simple-adhoc --sourceNode=20 --sinkNode=10"
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./waf --run "wifi-simple-adhoc-grid --verbose=1"
//
// By default, trace file writing is off-- to enable it, try:
// ./waf --run "wifi-simple-adhoc-grid --tracing=1"
//
// When you are done tracing, you will notice many pcap trace files
// in your directory.  If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-grid-0-0.pcap -nn -tt
//

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/netanim-module.h"

#include <stack> 
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhocGrid");

std::stack<uint64_t> uidStacks[100];

bool searchInStack(std::stack <uint64_t> s, uint64_t value) { 
  while (!s.empty()) { 
      uint64_t top = s.top();
      // NS_LOG_UNCOND("Search value " << value << " top value " << top);
      if(value == top) return true; 
      s.pop(); 
  } 
  return false;
}


static void GenerateTraffic (Ptr<Socket> socket, Ptr<Packet> packet){
  Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
  Ipv4Address ip_sender = iaddr.GetLocal (); 

  NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << ip_sender << "\tGoing to send packet with uid: " << packet->GetUid());
  uidStacks[socket->GetNode()->GetId ()].push(packet->GetUid());
  socket->Send (packet);
  
  // socket->Close ();
}


void ReceivePacket (Ptr<Socket> socket){ 
  Address from;
  Ipv4Address ip_sender;
  Ptr<Packet> pkt;

  while (pkt = socket->RecvFrom(from)){ 

    uint8_t *buffer = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buffer, pkt->GetSize ());
    std::string payload = std::string((char*)buffer);

    ip_sender = InetSocketAddress::ConvertFrom (from).GetIpv4 ();

    Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
    Ipv4Address ip_receiver = iaddr.GetLocal (); 

    Ipv4Address destination = ns3::Ipv4Address(payload.c_str());

    // NS_LOG_UNCOND("Receiver: " << ip_receiver << " Destination " << destination);
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ip_receiver << "\tReceived pkt size: " <<  pkt->GetSize () << " bytes with uid " << pkt->GetUid() << " from: " << ip_sender << " to: " << payload);

    if(ip_receiver != destination) {
      if (searchInStack(uidStacks[socket->GetNode()->GetId ()], pkt->GetUid()) == false){
            InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80); 
            socket->SetAllowBroadcast (true);
            socket->Connect (remote);
            
            GenerateTraffic(socket, pkt);
            /*Simulator::Schedule (Seconds (5.0), &GenerateTraffic,
                                socket, pkt);*/
      
            if (uidStacks[socket->GetNode()->GetId ()].size() >= 25) uidStacks[socket->GetNode()->GetId ()].pop();
      } else {
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ip_receiver << "\tI've already scheduled the message with uid: " << pkt->GetUid());
        // socket->Close ();
      }
    } else {
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<" I am " << ip_receiver << " finally receiverd the package with uid: " << pkt->GetUid() );
      // socket->Close ();
    }
  } 
}


int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double distance = 250;  // m
  //uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;
  uint32_t numNodes = 25;  // by default, 5x5
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  double interval = 25.0; // seconds
  bool verbose = false;
  bool tracing = false;

  double rss = -80;  // -dBm

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  //cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);

  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // FROM WIFI SIMPLE ADHOC GRID

  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (2) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // FROM WIFI SIMPLE ADHHOC - SEEMS TO BE WITHOUT LOSS
  /*
  // This is one parameter that matters when using FixedRssLossModel
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (0) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  // The below FixedRssLossModel will cause the rss to be fixed regardless
  // of the distance between the two stations, and the transmit power
  wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  wifiPhy.SetChannel (wifiChannel.Create ());
  */

  // Add an upper mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  MobilityHelper mobility;

  /*
   * WITHOUT GRID 
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  positionAlloc->Add (Vector (15.0, 0.0, 0.0));
  */

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  InternetStackHelper internet;
  // internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);

  Ptr<Socket> recvSinkArray[numNodes];
  for(uint32_t i=0; i<numNodes; ++i){
    uidStacks[c.Get (i)->GetId ()] = std::stack<uint64_t>();
    recvSinkArray[i] = Socket::CreateSocket (c.Get (i), tid);
    recvSinkArray[i]->Bind (local);
    recvSinkArray[i]->SetRecvCallback (MakeCallback (&ReceivePacket));
  }

  //InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  
  /*
  Ptr<Socket> sourceArray[numNodes];
  for(uint32_t i=0; i<numNodes; ++i){
    sourceArray[i] = Socket::CreateSocket (c.Get (i), tid);
    sourceArray[i]->SetAllowBroadcast (true);
    sourceArray[i]->Connect (remote);
    Simulator::Schedule (Seconds (5.0 * i), &GenerateTraffic,
                        sourceArray[i], packetSize, numPackets, interPacketInterval);
  }   
  */
  
  Ptr<Socket> source = Socket::CreateSocket (c.Get (sourceNode), tid);
  // InetSocketAddress remote = InetSocketAddress (i.GetAddress (sinkNode, 0), 80);
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  Ipv4InterfaceAddress iaddr = c.Get(sinkNode)->GetObject<Ipv4>()->GetAddress (1,0);
  Ipv4Address ip_receiver = iaddr.GetLocal ();

  std::ostringstream msg; msg << ip_receiver << '\0';
  uint32_t packetSize = msg.str().length()+1;  // where packetSize replace pktSize
  Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), packetSize);

  Simulator::Schedule (Seconds (30.0), &GenerateTraffic,
                       source, packet);

  // Output what we are doing
  // NS_LOG_UNCOND ("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance " << distance);

  int x=0, y=0;
  AnimationInterface anim("adhoc-grid.xml");
  for(uint32_t i=0; i<numNodes; ++i){
      if(i != 0 && i % 5 == 0) {
        x = 0;
        y += distance;
      }
      anim.SetConstantPosition(c.Get(i), x, y);
      // NS_LOG_UNCOND("Coordination for point " << i << " (" << x << ", "<< y << ")");
      x = x+distance;
  }
  
  anim.UpdateNodeDescription(c.Get(sourceNode),"Sender");
  anim.UpdateNodeDescription(c.Get(sinkNode),"Receiver");

  Simulator::Stop (Seconds (900.0));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

