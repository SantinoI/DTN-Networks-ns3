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

std::stack<uint64_t> uidStacks[800];

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
  // Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
  // Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
  // Ipv4Address ip_sender = iaddr.GetLocal (); 

  // NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << ip_sender << "\tGoing to send packet");
  socket->Send (packet);
  
  // socket->Close ();
}


void ReceivePacket (Ptr<Socket> socket){ 
  Address from;
  Ipv4Address ip_sender;
  Ptr<Packet> pkt;

  while (pkt = socket->RecvFrom(from)){ 

    ip_sender = InetSocketAddress::ConvertFrom (from).GetIpv4 ();

    Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
    Ipv4Address ip_receiver = iaddr.GetLocal (); 

    /*
     * EXAMPLE FROM SCRATCH
    std::string s = "scott>=tiger>=mushroom";
  std::string delimiter = ">=";

  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
      token = s.substr(0, pos);
      std::cout << token << std::endl;
      s.erase(0, pos + delimiter.length());
  }
  std::cout << s << std::endl;
  */

    // Following block of code to insert in class please
    uint8_t *buffer = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buffer, pkt->GetSize ());
    
    std::string payload = std::string((char*)buffer);
    NS_LOG_UNCOND("Il payload è :: " << payload);
    std::string delimiter =  ";";
      
    uint32_t TTL;
    uint32_t UID;
    std::string destinationAddress;
      
    size_t pos = 0;
    int iterationCounter = 0;
    std::string token;
    while ((pos = payload.find(delimiter)) != std::string::npos) {
        token = payload.substr(0, pos);
        if(iterationCounter == 0) destinationAddress = token;
        else TTL = std:: stoi(token);
        payload.erase(0, pos + delimiter.length());
        iterationCounter++;
    }
    UID = std:: stoi(payload);
    TTL -= 1;
    
    Ipv4Address destination = ns3::Ipv4Address(destinationAddress.c_str());
    //NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ip_receiver << "\tReceived pkt size: " <<  pkt->GetSize () << " bytes with uid " << UID<< " from: " << ip_sender << " to: " << destinationAddress);


    if(ip_receiver != destination) {
      if(TTL != 0){
        if (searchInStack(uidStacks[socket->GetNode()->GetId ()], UID) == false){
              InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80); 
              socket->SetAllowBroadcast (true);
              socket->Connect (remote);
              
              // Update packet with new TTL
              std::ostringstream msg; msg << destinationAddress << ';' << TTL << ";"<< UID;
              uint32_t packetSize = msg.str().length()+1;
              NS_LOG_UNCOND("IL payload E' : -----------------------"<< msg.str().c_str() );
              Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), packetSize);
              NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << ip_sender << "\tGoing to send packet with uid: " << UID << " and TTL " << TTL );
  
              uidStacks[socket->GetNode()->GetId ()].push(UID);
              // GenerateTraffic(socket, packet);
              Simulator::Schedule (Seconds (3.0 + socket->GetNode()->GetId ()), &GenerateTraffic,
                                  socket, packet);
        
              if (uidStacks[socket->GetNode()->GetId ()].size() >= 25) uidStacks[socket->GetNode()->GetId ()].pop();
        } else {
          NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ip_receiver << "\tI've already scheduled the message with uid: " << UID);
          // socket->Close ();
        }
      }else NS_LOG_UNCOND("TTL Scaduto");
    } else {
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<" I am " << ip_receiver << " finally receiverd the package with uid: " << UID );
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
  uint32_t numNodes = 250;  // by default, 5x5
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 159;
  double interval = 25.0; // seconds
  bool verbose = false;
  bool tracing = false;

  uint32_t TTL = 20;
  uint32_t UID = 0;

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
                                 "GridWidth", UintegerValue (25),
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

  std::ostringstream msg; msg << ip_receiver << ';' << TTL << ";" << UID;
  NS_LOG_UNCOND("First payload " << msg.str().c_str());
  uint32_t packetSize = msg.str().length()+1;  // where packetSize replace pktSize
  Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), packetSize);

  Simulator::Schedule (Seconds (30.0), &GenerateTraffic,
                       source, packet);

  int x=0, y=0;
  AnimationInterface anim("adhoc-grid.xml");
  for(uint32_t i=0; i<numNodes; ++i){
      if(i != 0 && i % 25 == 0) {
        x = 0;
        y += distance;
      }
      anim.SetConstantPosition(c.Get(i), x, y);
      x = x+distance;
  }
  
  anim.UpdateNodeDescription(c.Get(sourceNode),"Sender");
  anim.UpdateNodeDescription(c.Get(sinkNode),"Receiver");

  Simulator::Stop (Seconds (900.0));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

