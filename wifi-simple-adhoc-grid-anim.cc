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

typedef struct{
  bool delivered;
  double start;
  double delivered_at;
} PacketLogData;

std::string debugLevel = "EXTRACTOR"; //["NONE","NORMAL","MAX","EXTRACTOR"]

std::vector<PacketLogData> dataForPackets;

std::vector<std::string> splitString(std::string value, std::string delimiter){
  std::vector<std::string> values;
  size_t pos = 0;
  std::string token;
  while ((pos = value.find(delimiter)) != std::string::npos) {
      token = value.substr(0, pos);
      values.push_back(token);
      value.erase(0, pos + delimiter.length());
  }
  values.push_back(value);
  return values;
}

std::string createStringAddressUid(Ipv4Address address, int uid, std::string delimiter){
  std::ostringstream value;
  value << address << delimiter << uid;
  return value.str();
}

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

    void setDestinationAddressFromString(std::string value){ destinationAddress = ns3::Ipv4Address(value.c_str()); };

    void decreaseTtl(){ ttl -= 1; };

    void fromString(std::string stringPayload){
      if(type == EPIDEMIC){
          // 10.0.2.3;5;3 => IP;TTL;UID
          std::vector<std::string> values = splitString(stringPayload, delimiter);
          destinationAddress = ns3::Ipv4Address(values[0].c_str());
          ttl = std::stoi(values[1]);
          uid = std::stoi(values[2]);
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
      // 10.0.1.2;5 => IP;UID
      std::vector<std::string> values = splitString(value, ";");
      int uid = std::stoi(values[1]);

      std::stack<std::string> s = uidsPacketReceived;

      int counter = 0;
      while (!s.empty()) {
        std::string top = s.top();
        // 10.0.1.2;5 => IP;UID
        values = splitString(top, ";");
        int tempUid = std::stoi(values[1]);

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
    std::string pushInReceived(ns3::Ipv4Address previousAddress, int uid){
      std::string value = createStringAddressUid(previousAddress, uid, ";");
      uidsPacketReceived.push(value);
      return value;
    }
    void popFromStack(){ packetsScheduled.pop(); }
    void popFromReceived(){ uidsPacketReceived.pop(); }
};

std::vector<NodeHandler> nodeHandlerArray;

static void GenerateTraffic (Ptr<Socket> socket, Ptr<Packet> packet, uint32_t UID, std::string previousAddressUid, uint32_t ttl){
  Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
  Ipv4Address ipSender = iaddr.GetLocal ();

  NodeHandler* currentNode = &nodeHandlerArray[socket->GetNode()->GetId()];

  if(currentNode->searchInStack(UID) == false ||                 //stack of sent pkt
    (currentNode->searchInStack(UID) == true &&                  //stack of sent pkt
      (currentNode->countInReceived(previousAddressUid) < 2))    //stack of received pkt
    ){
    if(debugLevel == "EXTRACTOR"){NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT SENT, UID:    " << UID);}
    if(debugLevel == "NORMAL" or debugLevel == "MAX"){NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << ipSender <<"    " << socket->GetNode()->GetId() << "    Going to send packet with uid:    " << UID << "    and TTL:    " << ttl);}

    socket->Send (packet);
    if(dataForPackets[UID].start == 0) dataForPackets[UID].start = Simulator::Now().GetSeconds();

    currentNode->pushInStack(UID);
    currentNode->increaseBytesSent((double)packet->GetSize());
    currentNode->increasePacketsSent(1);

    Simulator::Schedule (Seconds(60), &GenerateTraffic, socket, packet, UID, previousAddressUid, ttl);
  }
}


void ReceivePacket (Ptr<Socket> socket){
  Address from;
  Ipv4Address ipSender;
  Ptr<Packet> pkt;

  while (pkt = socket->RecvFrom(from)){

    NodeHandler* currentNode = &nodeHandlerArray[socket->GetNode()->GetId()];

    currentNode->increaseBytesReceived((double)pkt->GetSize());
  	currentNode->increasePacketsReceived(1);

    ipSender = InetSocketAddress::ConvertFrom (from).GetIpv4 ();

    Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
    Ipv4Address ipReceiver = iaddr.GetLocal ();

    PayLoadConstructor payload = PayLoadConstructor(EPIDEMIC);
    payload.fromPacket(pkt);
    payload.decreaseTtl();

    uint32_t UID = payload.getUid();
    uint32_t TTL = payload.getTtl();
    Ipv4Address destinationAddress = payload.getDestinationAddress();

    std::string previousAddressUid = createStringAddressUid(destinationAddress, (int)UID, ";");

    if (currentNode->searchInReceived(previousAddressUid) == false) currentNode->pushInReceived(ipSender, UID);
    if(debugLevel == "EXTRACTOR"){NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT RECEIVED, UID:    " << UID);}
    if(debugLevel == "NORMAL" or debugLevel == "MAX"){NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ipReceiver << "    " << socket->GetNode()->GetId() << "    Received pkt size: " <<  pkt->GetSize () << " bytes with uid    " << UID << "    and TTL    " << TTL << "    from: " << ipSender << " to: " << destinationAddress);}

    if(ipReceiver != destinationAddress) {
      if(TTL != 0){
        if (currentNode->searchInStack(UID) == false){
              InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
              socket->SetAllowBroadcast (true);
              socket->Connect (remote);

              Ptr<Packet> packet = payload.toPacket();
              if(debugLevel == "MAX"){NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << ipSender << "\tGoing to RE-send packet with uid: " << UID << " and TTL " << TTL );}

              Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
              double randomPause = x->GetValue(0.1, 1.0);
              Simulator::Schedule (Seconds(randomPause), &GenerateTraffic,
                                  socket, packet, UID, previousAddressUid, TTL);

        } else {
          if(debugLevel == "MAX"){NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ipReceiver << "\tI've already scheduled the message with uid: " << UID);}
        }
      }
    } else {
      if (dataForPackets[UID].delivered != true){  // Prevent multiple logs for the same pkg receiver more times
        dataForPackets[UID].delivered = true;
        dataForPackets[UID].delivered_at = Simulator::Now().GetSeconds();
      if(debugLevel == "EXTRACTOR"){NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT DESTINATION REACHED, UID:    " << UID);}
      if(debugLevel == "NORMAL" or debugLevel == "MAX"){NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<"s I am " << ipReceiver << " finally received the package with uid:    " << UID );}
      } else {
      if(debugLevel == "MAX"){NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<"s I am " << ipReceiver << " finally received the package with uid: " << UID );}
      }
    }
  }
}


int main (int argc, char *argv[]){
  std::string phyMode ("DsssRate1Mbps");
  //uint32_t gridWidth = 10;
  // double distance = 150;  // m

  double simulationTime = 5000.00;
  uint32_t seed = 91;
  uint32_t sendAfter = 300;

  uint32_t numPackets = 2;
  uint32_t numNodes = 80;  // by default, 50
  uint32_t sinkNode = 45;
  uint32_t sourceNode = 44;

  uint32_t TTL = 10;
  uint32_t UID = 0;

  double rss = -80;  // -dBm

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  // cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);
  cmd.AddValue ("ttl", "TTL For each packet", TTL);
  cmd.AddValue ("seed", "Custom seed for simulation", seed);
  cmd.AddValue ("simulationTime", "Set a custom time (s) for simulation", simulationTime);
  cmd.AddValue ("sendAfter", "Send the first pkt after", sendAfter);

  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.Parse (argc, argv);

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  SeedManager::SetSeed(seed);

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
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  Ipv4InterfaceAddress iaddr = c.Get(sinkNode)->GetObject<Ipv4>()->GetAddress (1,0);
  Ipv4Address ipReceiver = iaddr.GetLocal ();

  Ipv4InterfaceAddress iaddrSender = c.Get(sourceNode)->GetObject<Ipv4>()->GetAddress (1,0);
  Ipv4Address ipSender = iaddrSender.GetLocal ();
  uint32_t tempTTL;
  for (uint32_t i=0; i<numPackets; i++){
    PayLoadConstructor payload = PayLoadConstructor(EPIDEMIC);
    tempTTL = TTL + (i*2);
    payload.setTtl(tempTTL);
    payload.setUid(UID);
    payload.setDestinationAddress(ipReceiver);
    Ptr<Packet> packet = payload.toPacket();

    PacketLogData dataPacket = {false, 0.00, 0.00};
    dataForPackets.push_back(dataPacket);

    Simulator::Schedule(Seconds(sendAfter * i), &GenerateTraffic,
                        source, packet, UID, createStringAddressUid(ipSender, (int)UID, ";"));
    UID += 1;
  }

  AnimationInterface anim("adhoc-grid.xml");
  anim.SetMaxPktsPerTraceFile(500000);

  anim.UpdateNodeDescription(c.Get(sourceNode),"Sender");
  anim.UpdateNodeDescription(c.Get(sinkNode),"Receiver");

  Simulator::Stop(Seconds(simulationTime));

  Simulator::Run ();
  Simulator::Destroy ();

  int deliveredCounter = 0;
  for (int i = 0; i < (int)dataForPackets.size(); i++) {
    if (dataForPackets[i].delivered == true) {
      deliveredCounter++;
      NS_LOG_UNCOND("- Packets " << i + 1 << " delta delivery: \t" << (double)(dataForPackets[i].delivered_at - dataForPackets[i].start));
    }
    else NS_LOG_UNCOND("- Packets " << i + 1 << " delta delivery: \t" << 0);
  }
  if(debugLevel != "NONE"){
    NS_LOG_UNCOND("- Packets sent: \t" << (int)dataForPackets.size());
    NS_LOG_UNCOND("- Packets delivered: \t" << deliveredCounter);
    NS_LOG_UNCOND("- Delivery percentage: \t" << ((double)deliveredCounter / (double)dataForPackets.size()) * 100.00 << "%");
    // Delivery time (?) (?) (?)
  }
  
  double totalBytesSent = 0.00;
  double totalBytesReceived = 0.00;
  int totalPacketsSent = 0;
  int totalPacketsReceived = 0;
  int totalAttempt = 0;
  for (uint32_t i = 0; i < numNodes; ++i){
    totalBytesSent += nodeHandlerArray[i].getBytesSent();
    totalBytesReceived += nodeHandlerArray[i].getBytesReceived();
    totalPacketsSent += nodeHandlerArray[i].getPacketsSent();
    totalPacketsReceived += nodeHandlerArray[i].getPacketsReceived();
    totalAttempt += nodeHandlerArray[i].getAttempt();
  }

  if(debugLevel != "NONE"){
    NS_LOG_UNCOND("- Total BytesSent: \t" << totalBytesSent);
    NS_LOG_UNCOND("- Total BytesReceived: \t" << totalBytesReceived);
    NS_LOG_UNCOND("- Total PacketsSent: \t" << totalPacketsSent);
    NS_LOG_UNCOND("- Total PacketsReceived: \t" << totalPacketsReceived);
    NS_LOG_UNCOND("- Total Attempt: \t" << totalAttempt);
  }
  return 0;
}
