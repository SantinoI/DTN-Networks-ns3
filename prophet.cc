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
  HELLO_ACK,
  HELLO_ACK2,
	PROPHET
};

typedef struct{
  bool delivered;
  double start;
  double delivered_at;
} PacketLogData;

// Struttura di incontri
typedef struct{
  Ipv4Address ip;
  double time_meet;
} Meet;

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
    int getType(){return type;};
    Ipv4Address getDestinationAddress(){ return destinationAddress; };

    void setTtl(uint32_t value){ ttl = value; };
    void setUid(uint32_t value){ uid = value; };
    void setDestinationAddress(Ipv4Address value){ destinationAddress = value; };

    // Ipv4Address getDestinationAddressAsString(){ return destinationAddress; };
    void setDestinationAddressFromString(std::string value){ destinationAddress = ns3::Ipv4Address(value.c_str()); };

    void decreaseTtl(){ ttl -= 1; };

    void fromString(std::string stringPayload){
      std::vector<std::string> values = splitString(stringPayload, delimiter);
      if(type == EPIDEMIC){
          // 10.0.2.3;5;3 => IP;TTL;UID
          destinationAddress = ns3::Ipv4Address(values[0].c_str());
          ttl = std::stoi(values[1]);
          uid = std::stoi(values[2]);
      }
      else if(type == HELLO){
          if(values[0] == "HELLO_ACK"){
            type = HELLO_ACK;
          }
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
      if(type == EPIDEMIC){
        msg << destinationAddress << delimiter << ttl << delimiter << uid;
      }
      else if(type == HELLO){
        msg << "HELLO"; 
      }
      else if(type == HELLO_ACK){
        msg << "HELLO_ACK";
      }

      return msg; 
      };

    Ptr<Packet> toPacket(){
      std::ostringstream msg = toString();
      uint32_t packetSize = msg.str().length()+1;
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), packetSize);
      return packet;
    }
    Ptr<Packet> toPacketFromString(std::ostringstream msg){
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
    std::stack<Meet> meeting;
    // Vettore delle predizioni. es A:23.8;B:96
    std::vector<std::string> predictability;

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

    std::ostringstream getPredictability(){
      std::ostringstream msg;
      for(int i = 0; i < (int)predictability.size(); i++){
        msg << predictability[i] << ";";
      }
      NS_LOG_UNCOND("predictability del nodo è " << msg.str());
      return msg;
    }

    void updatePredictability(int type,Ipv4Address ip){
      //std::vector<std::string> value = 

      if(type == HELLO){
        bool exist = false;
        for(int i = 0; i < (int)predictability.size(); i++){
          std::vector<std::string> oldValue = splitString(predictability[i], ":");
          if(ns3::Ipv4Address(oldValue[0].c_str()) == ip){
            exist = true;
            NS_LOG_UNCOND("Ho trovato un riscontro, sto aggiornando " << oldValue[0]);
            float newValue =  atof(oldValue[0].c_str()) + (1- atof(oldValue[0].c_str()))* 0.75;
            NS_LOG_UNCOND("Il nuovo valore è " << newValue);
            predictability.erase(predictability.begin()+i);
            std::ostringstream newEntry;
            newEntry << ip << ":" << newValue;
            predictability.push_back(newEntry.str());

          }
        }
        if(exist == false ){
          std::ostringstream newEntry;
          newEntry << ip << ":" << "0";
          predictability.push_back(newEntry.str());
        }
      }

    }
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
    void pushInMeeting(Ipv4Address ip, double time_meet){
      Meet meet = {ip,time_meet};
      if(meeting.size() == 30){
        meeting.pop();
      }
      meeting.push(meet);
    }
    void printMeeting(int io){
      std::stack<Meet> s = meeting;

      while (!s.empty()) {
        Meet top = s.top();
        NS_LOG_UNCOND("Io sono: " << io << " ho ricevuto l'ACK da  " << top.ip << " Al tempo " << top.time_meet); 
        s.pop();
      }
    }
};

std::vector<NodeHandler> nodeHandlerArray;

static void GenerateHello (Ptr<Socket> socket){
  Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
  Ipv4Address ipSender = iaddr.GetLocal ();

  NodeHandler* currentNode = &nodeHandlerArray[socket->GetNode()->GetId()];

    PayLoadConstructor payload = PayLoadConstructor(HELLO);
    Ptr<Packet> packet = payload.toPacket();

    NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s\t" << ipSender << "\tVado a mandare il pacchetto di HELLO: ");

    socket->Send(packet);
    
    currentNode->increaseBytesSent((double)packet->GetSize());
    currentNode->increasePacketsSent(1);

    Simulator::Schedule (Seconds(60), &GenerateHello, socket);
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

    //Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
    //Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
    //Ipv4Address ipReceiver = iaddr.GetLocal ();

    PayLoadConstructor payload = PayLoadConstructor(HELLO);
    payload.fromPacket(pkt);
    NS_LOG_UNCOND(socket->GetNode()->GetId()<<" Ho ricevuto un pacchetto al tempo " << Simulator::Now().GetSeconds()  << " DA " << ipSender << " " << payload.getType());

    if(payload.getType() == HELLO){
      
      currentNode->updatePredictability(payload.getType(),ipSender);
      std::ostringstream predicability = currentNode->getPredictability();
      PayLoadConstructor payload = PayLoadConstructor(HELLO_ACK);
      Ptr<Packet> packet = payload.toPacketFromString(predicability);
      // Invio ACK con la tabella del nodo attuale
      std::ostringstream msg = currentNode->getPredictability();
      uint32_t packetSize = msg.str().length()+1;
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), packetSize);
      InetSocketAddress remote = InetSocketAddress (ipSender, 80);
      socket->Connect (remote);
      socket->Send(packet);
    }
    else if(payload.getType() == HELLO_ACK){
      currentNode->pushInMeeting(ipSender, Simulator::Now().GetSeconds());
      currentNode->printMeeting(socket->GetNode()->GetId());


    }
    else if(payload.getType() == HELLO_ACK2){
      currentNode->pushInMeeting(ipSender, Simulator::Now().GetSeconds());
      currentNode->printMeeting(socket->GetNode()->GetId());
    }
  }
}

int main (int argc, char *argv[]){
  std::string phyMode ("DsssRate1Mbps");
  //uint32_t gridWidth = 10;
  // double distance = 150;  // m
  
  uint32_t numPackets = 1;
  uint32_t numNodes = 3;  // by default, 50
  uint32_t sinkNode = 1;
  uint32_t sourceNode = 2;

  uint32_t TTL = 6;
  //uint32_t UID = 0;

  double rss = -80;  // -dBm

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  // cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);
  cmd.AddValue ("ttl", "TTL For each packet", TTL);

  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.Parse (argc, argv);

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  SeedManager::SetSeed (91);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // FROM WIFI SIMPLE ADHOC GRID

  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (4) );
  wifiPhy.Set ("TxGain", DoubleValue (4) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());


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
                                "X", StringValue ("25.0"),
                                "Y", StringValue ("25.0"),
                                "Theta", StringValue ("ns3::UniformRandomVariable[Min=-10.0|Max=10.0]"),
                                "Rho", StringValue ("ns3::UniformRandomVariable[Min=10.0|Max=10.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                           	"Mode", StringValue ("Time"),
                            "Time", StringValue ("15s"),
                            "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),// 36 km/h
                            "Bounds", StringValue ("0|50|0|50"));

  mobility.InstallAll();

  InternetStackHelper internet;
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  InetSocketAddress broadcast = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
    

  Ptr<Socket> recvSinkArray[numNodes];
  for(uint32_t i=0; i<numNodes; ++i){
    nodeHandlerArray.push_back(*new NodeHandler());
    recvSinkArray[i] = Socket::CreateSocket (c.Get (i), tid);
    recvSinkArray[i]->Bind (local);
    recvSinkArray[i]->SetRecvCallback (MakeCallback (&ReceivePacket));

    recvSinkArray[i]->SetAllowBroadcast (true);
    recvSinkArray[i]->Connect (broadcast);
    
    Simulator::Schedule(Seconds(10 * i), &GenerateHello,
                        recvSinkArray[i]);
  }


  AnimationInterface anim("adhoc-grid.xml");

  anim.UpdateNodeDescription(c.Get(sourceNode),"Sender");
  anim.UpdateNodeDescription(c.Get(sinkNode),"Receiver");

  Simulator::Stop (Seconds (500.0));

  Simulator::Run ();
  Simulator::Destroy ();

  int deliveredCounter = 0;
  for (int i = 0; i < (int)dataForPackets.size(); i++){
    if(dataForPackets[i].delivered == true) deliveredCounter++;
  }
  NS_LOG_UNCOND("- Packets sent: \t" << (int)dataForPackets.size());
  NS_LOG_UNCOND("- Packets delivered: \t" << deliveredCounter);
  NS_LOG_UNCOND("- Delivery percentage: \t" << ((double)deliveredCounter / (double)dataForPackets.size()) * 100.00 << "%");
  // Delivery time (?) (?) (?)

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

  NS_LOG_UNCOND("- Total BytesSent: \t" << totalBytesSent);
  NS_LOG_UNCOND("- Total BytesReceived: \t" << totalBytesReceived);
  NS_LOG_UNCOND("- Total PacketsSent: \t" << totalPacketsSent);
  NS_LOG_UNCOND("- Total PacketsReceived: \t" << totalPacketsReceived);
  NS_LOG_UNCOND("- Total Attempt: \t" << totalAttempt);

  return 0;
}