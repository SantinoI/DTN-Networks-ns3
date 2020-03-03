//TODO: try with different Time in RandomWalk2dMobilityModel like 60s or so
//TODO: try to develop with 2 different receiver(hack sender) instead of 2 times received(hack)
#include <stack>
#include <vector>
#include <algorithm>

#include "ns3/core-module.h"
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
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhocGrid");

// Define enumeration for PayLoad type
enum {
	EPIDEMIC,
	HELLO,
	PROPHET
};

class PayLoadConstructor{
  private:
  	int type;
    uint32_t ttl;
    uint32_t uid;
    Ipv4Address destinationAddress;
    std::string delimiter;

  public:
    PayLoadConstructor(int _type){
      delimiter =  ";";
      type = _type;
    }

    uint32_t getTtl(){ return ttl; };
    uint32_t getUid(){ return uid; };
    Ipv4Address getDestinationAddress(){ return destinationAddress; };

    void setTtl(uint32_t value){ ttl = value; };
    void setUid(uint32_t value){ uid = value; };
    void setDestinationAddress(Ipv4Address value){ destinationAddress = value; };

    // Ipv4Address getDestinationAddressAsString(){ return destinationAddress; };
    void setDestinationAddressFromString(std::string value){ destinationAddress = ns3::Ipv4Address(value.c_str()); };

    void decreaseTtl(){ ttl -= 1; };

    void fromString(std::string stringPayload){
      size_t pos = 0;
      int iterationCounter = 0;
      std::string token;
      if(type == EPIDEMIC){
            while ((pos = stringPayload.find(delimiter)) != std::string::npos) {
                token = stringPayload.substr(0, pos);
                if(iterationCounter == 0) destinationAddress = ns3::Ipv4Address(token.c_str());  // destinationAddress = token;
                else ttl = std:: stoi(token);
                stringPayload.erase(0, pos + delimiter.length());
                iterationCounter++;
            }
            uid = std:: stoi(stringPayload);
      }
    }

    void fromPacket(Ptr<Packet> packet){
      uint8_t *buffer = new uint8_t[packet->GetSize ()];
      packet->CopyData(buffer, packet->GetSize ());
      std::string stringPayload = std::string((char*)buffer);

      fromString(stringPayload);
    };

    std::ostringstream toString(){
      std::ostringstream msg;
      msg << destinationAddress << delimiter << ttl << delimiter << uid;
      return msg;
    };

    Ptr<Packet> toPacket(){
      std::ostringstream msg = toString();
      uint32_t packetSize = msg.str().length()+1;
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), packetSize);
      return packet;
    }
};

class NodeHandler{
  private:
    double bytesSent;
    int packetsSent;
    double bytesReceived;
    int packetsReceived;
    int attempt;
    std::stack<uint64_t> packetsScheduled;
    std::stack<std::string> uidsPacketReceived;

    // Next routing table values

  public:
    NodeHandler(){
      bytesSent = 0.00;
      packetsSent = 0;
      bytesReceived = 0.0;
      packetsReceived = 0;
      attempt = 0;
    }
    double getBytesSent(){ return bytesSent; }
    int getPacketsSent(){ return packetsSent; }

    void setBytesSent(double value){ bytesSent = value; }
    void setPacketsSent(double value){ packetsSent = value; }

    double getBytesReceived(){ return bytesReceived; }
    int getPacketsReceived(){ return packetsReceived; }

    void incrementAttempt(){ attempt++; }
    int getAttempt(){ return attempt; }

    void setBytesReceived(double value){ bytesReceived = value; }
    void setPacketsReceived(double value){ packetsReceived = value; }

    void increaseBytesSent(double value){ bytesSent += value; }
    void increasePacketsSent(double value){ packetsSent += value; }
    void increaseBytesReceived(double value){ bytesReceived += value; }
    void increasePacketsReceived(double value){ packetsReceived += value; }

    bool searchInStack(uint64_t value){
        std::stack<uint64_t> s = packetsScheduled;
        while (!s.empty()) {
          uint64_t top = s.top();
          if(value == top) return true;
          s.pop();
        }
        return false;
      }
    int countInReceived(std::string value){
      size_t pos = 0;
      int iterationCounter = 0;
      std::string token;
      ns3::Ipv4Address previousAddress;
      int uid;
      std::string delimiter = ";";
      while ((pos = value.find(delimiter)) != std::string::npos) {
          token = value.substr(0, pos);
          if(iterationCounter == 0) previousAddress = ns3::Ipv4Address(token.c_str());  // destinationAddress = token;
          value.erase(0, pos + delimiter.length());
          iterationCounter++;
      }
      uid = std:: stoi(value);


      std::stack<std::string> s = uidsPacketReceived;

      int counter = 0;
      while (!s.empty()) {
        std::string top = s.top();
        pos = 0;
        iterationCounter = 0;
        ns3::Ipv4Address tempPreviousAddress;
        int tempUid;
        while ((pos = value.find(delimiter)) != std::string::npos) {
            token = value.substr(0, pos);
            if(iterationCounter == 0) tempPreviousAddress = ns3::Ipv4Address(token.c_str());  // destinationAddress = token;
            value.erase(0, pos + delimiter.length());
            iterationCounter++;
        }
        tempUid = std:: stoi(value);
        if( uid == tempUid ) counter++;
        s.pop();
      }
      return counter;
    }
    bool searchInReceived(std::string value){
      std::stack<std::string> s = uidsPacketReceived;

      while (!s.empty()) {
        std::string top = s.top();
        if( top == value) return true;
        s.pop();
      }
      return false;
    }

    void pushInStack(uint64_t value){ packetsScheduled.push(value); }
    std::string pushInReceived(ns3::Ipv4Address previousAddress,int uid){
      std::ostringstream value;
      value << previousAddress << ";" << uid;
      uidsPacketReceived.push(value.str());
      return value.str();
    }
    void popFromStack(){ packetsScheduled.pop(); }
    void popFromReceived(){ uidsPacketReceived.pop(); }
};

std::vector<NodeHandler> nodeHandlerArray;

static void GenerateTraffic (Ptr<Socket> socket, Ptr<Packet> packet, uint32_t UID, std::string previousAddress_uid){
  // Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
  // Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
  // Ipv4Address ip_sender = iaddr.GetLocal ();


  if(nodeHandlerArray[socket->GetNode()->GetId()].searchInStack(UID) == false or //stack of sent pkt
    (nodeHandlerArray[socket->GetNode()->GetId()].searchInStack(UID) == true and //stack of sent pkt
      (nodeHandlerArray[socket->GetNode()->GetId()].countInReceived(previousAddress_uid) < 2))   //stack of received pkt
    ){
    NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << "\tGoing to send packet");
    socket->Send (packet);
    nodeHandlerArray[socket->GetNode()->GetId()].pushInStack(UID);
    nodeHandlerArray[socket->GetNode()->GetId()].increaseBytesSent((double)packet->GetSize());
    nodeHandlerArray[socket->GetNode()->GetId()].increasePacketsSent(1);

    Simulator::Schedule (Seconds(60), &GenerateTraffic, socket, packet, UID, previousAddress_uid);
  }

  // socket->Close ();
}


void ReceivePacket (Ptr<Socket> socket){
  Address from;
  Ipv4Address ip_sender;
  Ptr<Packet> pkt;

  while (pkt = socket->RecvFrom(from)){

    nodeHandlerArray[socket->GetNode()->GetId()].increaseBytesReceived((double)pkt->GetSize());
  	nodeHandlerArray[socket->GetNode()->GetId()].increasePacketsReceived(1);


    ip_sender = InetSocketAddress::ConvertFrom (from).GetIpv4 ();

    Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
    Ipv4Address ip_receiver = iaddr.GetLocal ();

    PayLoadConstructor payload = PayLoadConstructor(EPIDEMIC);
    payload.fromPacket(pkt);
    payload.decreaseTtl();
    // Only for clear code, nothing else for the moment
    uint32_t UID = payload.getUid();
    uint32_t TTL = payload.getTtl();
    Ipv4Address destinationAddress = payload.getDestinationAddress();
    std::ostringstream value;
    value << ip_sender << ";" << UID;
    std::string previousAddress_uid = value.str();

    if (nodeHandlerArray[socket->GetNode()->GetId()].searchInReceived(previousAddress_uid) == false){nodeHandlerArray[socket->GetNode()->GetId()].pushInReceived(ip_sender,UID);}

    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ip_receiver << "  " << socket->GetNode()->GetId() << "\tReceived pkt size: " <<  pkt->GetSize () << " bytes with uid " << UID << " and TTL " << TTL << " from: " << ip_sender << " to: " << destinationAddress);

    if(ip_receiver != destinationAddress) {
      if(TTL != 0){
        if (nodeHandlerArray[socket->GetNode()->GetId()].searchInStack(UID) == false){
              InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
              socket->SetAllowBroadcast (true);
              socket->Connect (remote);

              // Update packet with new TTL
              // std::ostringstream msg; msg << destinationAddress << ';' << TTL << ";"<< UID;
              // uint32_t packetSize = msg.str().length()+1;
              Ptr<Packet> packet = payload.toPacket();
              NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << ip_sender << "\tGoing to send packet with uid: " << UID << " and TTL " << TTL );

              Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
              double randomPause = x->GetValue(0.1, 1.0);
              // NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\tNodo numero: " << socket->GetNode()->GetId() << " attesa di " << randomPause );
              Simulator::Schedule (Seconds(randomPause), &GenerateTraffic,
                                  socket, packet, UID, previousAddress_uid);

        } else {
          NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ip_receiver << "\tI've already scheduled the message with uid: " << UID);
          // socket->Close ();
        }
      } // else NS_LOG_UNCOND("TTL Scaduto");
    } else {
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<" I am " << ip_receiver << " finally received the package with uid: " << UID );
      // socket->Close ();
    }
  }
}


int main (int argc, char *argv[]){
  std::string phyMode ("DsssRate1Mbps");
  double distance = 150;  // m
  //uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 2;
  //uint32_t gridWidth = 10;
  uint32_t numNodes = 80;  // by default, 50
  uint32_t sinkNode = 45;
  uint32_t sourceNode = 44;
  double interval = 25.0; // seconds
  bool verbose = false;
  bool tracing = false;

  uint32_t TTL = 6;
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

  SeedManager::SetSeed (167);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // FROM WIFI SIMPLE ADHOC GRID

  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (-1) );
  wifiPhy.Set ("TxGain", DoubleValue (-1) );
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

  /*
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (gridWidth),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);
  */

  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                "X", StringValue ("5000.0"),
                                "Y", StringValue ("5000.0"),
                                "Theta", StringValue ("ns3::UniformRandomVariable[Min=-1000.0|Max=1000.0]"),
                                "Rho", StringValue ("ns3::UniformRandomVariable[Min=1000.0|Max=5000.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                           	"Mode", StringValue ("Time"),
                            "Time", StringValue ("15s"),
                            "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),// 36 km/h
                            "Bounds", StringValue ("0|10000|0|10000"));

  mobility.InstallAll();

  InternetStackHelper internet;
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);

  Ptr<Socket> recvSinkArray[numNodes];
  for(uint32_t i=0; i<numNodes; ++i){
    nodeHandlerArray.push_back(*new NodeHandler());
    recvSinkArray[i] = Socket::CreateSocket (c.Get (i), tid);
    recvSinkArray[i]->Bind (local);
    recvSinkArray[i]->SetRecvCallback (MakeCallback (&ReceivePacket));
  }

  Ptr<Socket> source = Socket::CreateSocket (c.Get (sourceNode), tid);
  // InetSocketAddress remote = InetSocketAddress (i.GetAddress (sinkNode, 0), 80);
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  Ipv4InterfaceAddress iaddr = c.Get(sinkNode)->GetObject<Ipv4>()->GetAddress (1,0);
  Ipv4Address ip_receiver = iaddr.GetLocal ();

  Ipv4InterfaceAddress iaddrSender = c.Get(sourceNode)->GetObject<Ipv4>()->GetAddress (1,0);
  Ipv4Address ip_sender = iaddrSender.GetLocal ();

  PayLoadConstructor payload;
  for (int uint32_t = 0; i < numPackets; i++){
    payload = PayLoadConstructor(EPIDEMIC);
    payload.setTtl(TTL);
    payload.setUid(UID);
    payload.setDestinationAddress(ip_receiver);
    Ptr<Packet> packet = payload.toPacket();

    std::ostringstream value;
    value << ip_sender << ";" << UID;

    Simulator::Schedule(Seconds(300 * i), &GenerateTraffic,
                        source, packet, UID, value.str());

    UID += 1;
  }

  AnimationInterface anim("adhoc-grid.xml");

  /*
    int x=0, y=0;
    for(uint32_t i=0; i<numNodes; ++i){
      if((i != 0) && (i % gridWidth == 0)) {
        x = 0;
        y += distance;
      }
      anim.SetConstantPosition(c.Get(i), x, y);
      x = x+distance;
  }
  */
  anim.UpdateNodeDescription(c.Get(sourceNode),"Sender");
  anim.UpdateNodeDescription(c.Get(sinkNode),"Receiver");

  Simulator::Stop (Seconds (7230.0));

  Simulator::Run ();
  Simulator::Destroy ();

  /*
  for(uint32_t i=0; i<numNodes; ++i){
      NS_LOG_UNCOND("Il nodo: " << i << " ha inviato: " << nodeHandlerArray[i].getPacketsSent() << " pacchetti per " << nodeHandlerArray[i].getBytesSent() << "bytes.");
  }
  */

  return 0;
}
