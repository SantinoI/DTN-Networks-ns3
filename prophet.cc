//TODO: try with different Time in RandomWalk2dMobilityModel like 60s or so
//TODO: try to develop with 2 different receiver(hack sender) instead of 2 times received(hack)
#include <algorithm>
#include <stack>
#include <vector>

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleAdhocGrid");

// Define enumeration for PayLoad type
enum {
    STANDARD,

    HELLO,
    HELLO_ACK,
    HELLO_ACK2,

    PKTREQ,
    PKTACK
};

typedef struct {
    bool delivered;
    double start;
    double delivered_at;
    int hops;
} PacketLogData;

// Struttura di incontri
typedef struct {
    Ipv4Address ip;
    float time_meet;
} Meet;

std::vector<PacketLogData> dataForPackets;

std::vector<std::string> splitString(std::string value, std::string delimiter) {
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

std::string createStringAddressUid(Ipv4Address address, int uid, std::string delimiter) {
    std::ostringstream value;
    value << address << delimiter << uid;
    return value.str();
}

class PayLoadConstructor {
   private:
    int type;
    uint32_t hops;
    uint32_t uid;
    Ipv4Address destinationAddress;
    std::string delimiter;

   public:
    PayLoadConstructor(int _type) {
        delimiter = ";";
        type = _type;
    }

    uint32_t getHops() { return hops; };
    uint32_t getUid() { return uid; };
    int getType() { return type; };
    Ipv4Address getDestinationAddress() { return destinationAddress; };

    void setHops(uint32_t value) { hops = value; };
    void setUid(uint32_t value) { uid = value; };
    void setType(int value) { type = value; };
    void setDestinationAddress(Ipv4Address value) { destinationAddress = value; };

    // Ipv4Address getDestinationAddressAsString(){ return destinationAddress; };
    void setDestinationAddressFromString(std::string value) { destinationAddress = ns3::Ipv4Address(value.c_str()); };

    void incrementHops() { hops += 1; };

    void fromString(std::string stringPayload) {
        std::vector<std::string> values = splitString(stringPayload, delimiter);
        if (type == STANDARD || type == PKTREQ || type == PKTACK) {
            // 3;10.0.2.3;5;3 => TYPE;IP;TTL;UID
            type = std::stoi(values[0]);
            destinationAddress = ns3::Ipv4Address(values[1].c_str());
            hops = std::stoi(values[2]);
            uid = std::stoi(values[3]);
        } else if (type == HELLO) {
            if (values[0] == "HELLO_ACK") type = HELLO_ACK;
        }
    }

    void fromPacket(Ptr<Packet> packet) {
        uint8_t *buffer = new uint8_t[packet->GetSize()];
        packet->CopyData(buffer, packet->GetSize());
        std::string stringPayload = std::string((char *)buffer);

        fromString(stringPayload);
    };

    std::ostringstream toString() {
        std::ostringstream msg;
        msg << destinationAddress << delimiter << hops << delimiter << uid;
        return msg;
    };

    Ptr<Packet> toPacket() {
        std::ostringstream msg = toString();
        uint32_t packetSize = msg.str().length() + 1;
        Ptr<Packet> packet = Create<Packet>((uint8_t *)msg.str().c_str(), packetSize);
        return packet;
    }
    Ptr<Packet> toPacketFromString(std::ostringstream &tmp) {
        // NS_LOG_UNCOND("toPacketFromString ha ricevuto " << tmp.str());
        std::ostringstream msg;
        msg << getType() << ";" << tmp.str();
        // NS_LOG_UNCOND("toPacketFromString sta impacchettando  IL MESSAGGIO " << msg.str());
        uint32_t packetSize = msg.str().length() + 1;
        Ptr<Packet> packet = Create<Packet>((uint8_t *)msg.str().c_str(), packetSize);
        return packet;
    }
};

class NodeHandler {
   private:
    int nodeid;

    double bytesSent;
    int packetsSent;
    double bytesReceived;
    int packetsReceived;

    double helloBytesSent;
    int helloPacketsSent;
    double helloBytesReceived;
    int helloPacketsReceived;

    int attempt;
    float lastAging;
    std::stack<uint64_t> packetsScheduled;
    std::stack<std::string> uidsPacketReceived;
    std::vector<Meet> meeting;
    // Vettore delle predizioni. es A:23.8;B:96
    std::vector<std::string> predictability;
    std::vector<PayLoadConstructor> bufferPackets;

   public:
    NodeHandler(int _nodeid) {
        nodeid = _nodeid;
        bytesSent = 0.00;
        packetsSent = 0;
        bytesReceived = 0.0;
        packetsReceived = 0;
        attempt = 0;

        helloBytesSent = 0.0;
        helloPacketsSent = 0;
        helloBytesReceived = 0.0;
        helloPacketsReceived = 0;
    }
    double getBytesSent() { return bytesSent; }
    int getPacketsSent() { return packetsSent; }
    double getBytesReceived() { return bytesReceived; }
    int getPacketsReceived() { return packetsReceived; }

    void setBytesSent(double value) { bytesSent = value; }
    void setPacketsSent(double value) { packetsSent = value; }
    void setBytesReceived(double value) { bytesReceived = value; }
    void setPacketsReceived(double value) { packetsReceived = value; }

    void increaseBytesSent(double value) { bytesSent += value; }
    void increasePacketsSent(double value) { packetsSent += value; }
    void increaseBytesReceived(double value) { bytesReceived += value; }
    void increasePacketsReceived(double value) { packetsReceived += value; }

    double getHelloBytesSent() { return helloBytesSent; }
    int getHelloPacketsSent() { return helloPacketsSent; }
    double getHelloBytesReceived() { return helloBytesReceived; }
    int getHelloPacketsReceived() { return helloPacketsReceived; }

    void increaseHelloBytesSent(double value) { helloBytesSent += value; }
    void increaseHelloPacketsSent(double value) { helloPacketsSent += value; }
    void increaseHelloBytesReceived(double value) { helloBytesReceived += value; }
    void increaseHelloPacketsReceived(double value) { helloPacketsReceived += value; }

    void incrementAttempt() { attempt++; }
    int getAttempt() { return attempt; }

    float getLasMeeting(float now, Ipv4Address ip) {
        for (int i = 0; i < (int)meeting.size(); i++) {
            if (meeting[i].ip == ip) {
                float diff = now - meeting[i].time_meet;
                meeting[i].time_meet = now;
                return diff;
            }
        }
        Meet meet = {ip, now};
        meeting.push_back(meet);
        return now;
    }

    std::ostringstream getPredictability() {
        std::ostringstream msg;
        for (int i = 0; i < (int)predictability.size(); i++) {
            msg << predictability[i] << ";";
        }
        //NS_LOG_UNCOND("predictability del nodo è " << msg.str());
        return msg;
    }
    std::vector<std::string> getPredictabilityAsArray() { return predictability; };
    void printPredictability(int nodeid) {
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\tPredictability of node: " << nodeid);
        NS_LOG_UNCOND("_________________________________");
        NS_LOG_UNCOND("| ADDRESS \t || PREDICT \t|");
        NS_LOG_UNCOND("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
        for (int i = 0; i < (int)predictability.size(); i++) {
            std::vector<std::string> singlePredict = splitString(predictability[i], ":");
            NS_LOG_UNCOND("| " << singlePredict[0] << " \t || " << singlePredict[1]);
        }
        NS_LOG_UNCOND("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    }

    void updatePredictability(Ptr<Packet> packet, Ipv4Address ip, Ipv4Address currentIp) {
        uint8_t *buffer = new uint8_t[packet->GetSize()];
        packet->CopyData(buffer, packet->GetSize());
        std::string stringPayload = std::string((char *)buffer);
        std::vector<std::string> payloadData = splitString(stringPayload, ";");
        int type = atoi(payloadData[0].c_str());

        // if (nodeid == 7 || nodeid == 81) NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << nodeid << " | " << currentIp << " Payload ricevuto " << stringPayload << " da: " << ip);

        if (type == HELLO) {
            bool exist = false;
            for (int i = 0; i < (int)predictability.size(); i++) {
                std::vector<std::string> oldValue = splitString(predictability[i], ":");
                if (ns3::Ipv4Address(oldValue[0].c_str()) == ip) {
                    exist = true;
                    //NS_LOG_UNCOND("Ho trovato un riscontro, sto aggiornando " << oldValue[0]);
                    float newValue = atof(oldValue[1].c_str()) + (1 - atof(oldValue[1].c_str())) * 0.75;
                    //NS_LOG_UNCOND("Il nuovo valore è " << newValue);
                    //predictability.erase(predictability.begin()+i);
                    std::ostringstream newEntry;
                    newEntry << ip << ":" << newValue;
                    //predictability.push_back(newEntry.str());
                    predictability.at(i) = newEntry.str();
                    break;
                }

            }
            if (exist == false) {
                std::ostringstream newEntry;
                newEntry << ip << ":" << "0.75";
                predictability.push_back(newEntry.str());
            }
        } else if (type == HELLO_ACK) {
            // Aggiorno il tempo trascorso dall'ultimo invecchiamento
            float deltaAging = getLasMeeting(Simulator::Now().GetSeconds(), ip);
            bool exist = false;

            for (int i = 0; i < (int)predictability.size(); i++) {
                std::vector<std::string> oldValue = splitString(predictability[i], ":");

                if (ns3::Ipv4Address(oldValue[0].c_str()) != ip) {
                    //if(ns3::Ipv4Address(oldValue[0].c_str()) == ip && i == (int)predictability.size()) break;
                    float newValue = atof(oldValue[1].c_str()) * (pow(0.98, deltaAging));
                    //NS_LOG_UNCOND("INVECCHIAMENTO: il vecchio valore è " << oldValue[1].c_str() << " Il nuovo è " << newValue);
                    //predictability.erase(predictability.begin()+i);
                    std::ostringstream newEntry;
                    newEntry << oldValue[0] << ":" << newValue;
                    // NS_LOG_UNCOND("New entry: " << newEntry.str());
                    //predictability.insert(predictability.begin()+i,newEntry.str());
                    predictability[i] = newEntry.str();
                }else exist = true;
            }
            if (exist == false) {
                std::ostringstream newEntry;
                newEntry << ip << ":" << "0.75";
                predictability.push_back(newEntry.str());
            }
        }
        // Itera l'array predictability del nodo corrente. Dentro si itera PayloadData,
        // Predict[i] e payloadData[i] -- > split con :
        // M = Nodo corrente , E = ip (nodo incontrato), D = tutti gli altri
        if (type == HELLO_ACK2 || type == HELLO_ACK) {
            float newValue;

            for (int x = 0; x < (int)predictability.size(); x++) {  // serve per prendere P(M,E)
                std::vector<std::string> currentValue = splitString(predictability[x], ":");
                if (ns3::Ipv4Address(currentValue[0].c_str()) == ip) {
                    //value found, save and break
                    newValue = atof(currentValue[1].c_str());
                    break;
                }
            }
            // P(M,D)new = P(M,D)old + (1 - P(M,D)old) * P(M,E) * P(E,D) * β where β is a scaling constant.
            // oldValue = P(M,D)old ; newValue = P(M,E) ; recValue = P(E,D)
            int lenPredictability = (int)predictability.size();
            for (int j = 1; j < (int)payloadData.size() - 1; j++) {
                std::vector<std::string> recValue = splitString(payloadData[j], ":");
                for (int i = 0; i < lenPredictability; i++) {
                    std::vector<std::string> oldValue = splitString(predictability[i], ":");
                    if (currentIp != ns3::Ipv4Address(recValue[0].c_str())){ //attuo tutto il procedimento solo se l'ip che sto controllando non è il mio
                        // NS_LOG_UNCOND("oldValue " << oldValue[0].c_str() << " recValue " << recValue[0].c_str());
                        if (ns3::Ipv4Address(oldValue[0].c_str()) == ns3::Ipv4Address(recValue[0].c_str())) {
                            float transValue = atof(oldValue[1].c_str()) + (1 - atof(oldValue[1].c_str())) * newValue * atof(recValue[1].c_str()) * 0.25;  // i dont know how much is Beta

                            if (transValue > atof(oldValue[1].c_str())) {  //non so se questo controllo si deve fare onestamente, ad intuito direi di si (me lo prendo solo se migliore di quello che ho già)
                                std::ostringstream newEntry;
                                newEntry << oldValue[0] << ":" << transValue;
                                // NS_LOG_UNCOND("1) New entry: " << newEntry.str());
                                predictability[i] = newEntry.str();
                            }
                            break;
                        } else if (i == ((int)predictability.size()) - 1) {
                            //New record to be added
                            std::ostringstream newEntry;

                            float transValue = newValue * atof(recValue[1].c_str()) * 0.25;
                            newEntry << recValue[0] << ":" << transValue;
                            // NS_LOG_UNCOND("2) New entry: " << newEntry.str());
                            predictability.push_back(newEntry.str());
                        } 
                    }
                }
            }
            // NS_LOG_UNCOND("FINE AGGIORNAMENTO DOPO TRANSITIVA");
            // printPredictability(nodeid);
        }
    }

    bool searchInStack(uint64_t value) {
        std::stack<uint64_t> s = packetsScheduled;
        while (!s.empty()) {
            uint64_t top = s.top();
            if (value == top) return true;
            s.pop();
        }
        return false;
    }
    int countInReceived(std::string value) {
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

            if (uid == tempUid) counter++;
            s.pop();
        }
        return counter;
    }
    bool searchInReceived(std::string value) {
        std::stack<std::string> s = uidsPacketReceived;

        while (!s.empty()) {
            std::string top = s.top();
            if (top == value) return true;
            s.pop();
        }
        return false;
    }

    void pushInStack(uint64_t value) { packetsScheduled.push(value); }
    std::string pushInReceived(ns3::Ipv4Address previousAddress, int uid) {
        std::string value = createStringAddressUid(previousAddress, uid, ";");
        uidsPacketReceived.push(value);
        return value;
    }
    void popFromStack() { packetsScheduled.pop(); }
    void popFromReceived() { uidsPacketReceived.pop(); }

    void savePacketsInBuffer(PayLoadConstructor payload) {
        payload.setType(STANDARD);
        bufferPackets.push_back(payload);
    }
    std::vector<PayLoadConstructor> getPacketsBuffer() {
        return bufferPackets;
    }
    void removePacketFromBufferByIndex(int index) {
        bufferPackets.erase(bufferPackets.begin() + index);
    }
};

std::vector<NodeHandler> nodeHandlerArray;

static void GenerateHello(Ptr<Socket> socket) {
    // Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
    // Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
    // Ipv4Address ipSender = iaddr.GetLocal();

    // NodeHandler *currentNode = &nodeHandlerArray[socket->GetNode()->GetId()];

    InetSocketAddress broadcast = InetSocketAddress(Ipv4Address("255.255.255.255"), 80);
    socket->SetAllowBroadcast(true);
    socket->Connect(broadcast);

    std::ostringstream msg;
    msg << HELLO;
    uint32_t packetSize = msg.str().length() + 1;
    Ptr<Packet> packet = Create<Packet>((uint8_t *)msg.str().c_str(), packetSize);

    // NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << ipSender << "\t Manda messaggio di HELLO: ");

    socket->Send(packet);

    Simulator::Schedule(Seconds(60), &GenerateHello, socket);
}

void ReceivePacket(Ptr<Socket> socket) {
    Address from;
    Ipv4Address ipSender;
    Ptr<Packet> pkt;

    while (pkt = socket->RecvFrom(from)) {
        NodeHandler *currentNode = &nodeHandlerArray[socket->GetNode()->GetId()];

        //currentNode->increaseBytesReceived((double)pkt->GetSize()); DA INSERIRE NEI VARI CASI (HELLO/NON HELLO) una volta fatta la struttura di invio msg
        //currentNode->increasePacketsReceived(1);

        ipSender = InetSocketAddress::ConvertFrom(from).GetIpv4();

        Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
        Ipv4Address ipReceiver = iaddr.GetLocal();

        // Apro il pacchetto
        uint8_t *buffer = new uint8_t[pkt->GetSize()];
        pkt->CopyData(buffer, pkt->GetSize());
        std::string stringPayload = std::string((char *)buffer);
        std::vector<std::string> value = splitString(stringPayload, ";");

        int originalPayloadType = atoi(value[0].c_str());
        PayLoadConstructor payload = PayLoadConstructor(atoi(value[0].c_str()));

        payload.fromPacket(pkt);

        if (payload.getType() == HELLO) {
            currentNode->updatePredictability(pkt, ipSender, ipReceiver);
            std::ostringstream predictability = currentNode->getPredictability();
            payload.setType(HELLO_ACK);
            // NS_LOG_UNCOND("Io sono -> " << ipReceiver << " Ho ricevuto un HELLO da " << ipSender << " sto mandando alla funzione toPacketFromString: -> " << predictability.str());
            Ptr<Packet> packet = payload.toPacketFromString(predictability);
            InetSocketAddress remote = InetSocketAddress(ipSender, 80);
            socket->Connect(remote);
            // NS_LOG_UNCOND("Io sono-> " << ipReceiver << " sto inviando l'ACK a " << ipSender << "Con la mia tabella uguale a: " << predictability.str());
            socket->Send(packet);
        } else if (payload.getType() == HELLO_ACK) {
            // NS_LOG_UNCOND("Sono " << ipReceiver << " HO RICEVUTO ACK");
            currentNode->updatePredictability(pkt, ipSender, ipReceiver);

            std::ostringstream predictability = currentNode->getPredictability();

            payload.setType(HELLO_ACK2);
            Ptr<Packet> packet = payload.toPacketFromString(predictability);
            InetSocketAddress remote = InetSocketAddress(ipSender, 80);
            socket->Connect(remote);
            socket->Send(packet);
            /*
            currentNode->pushInMeeting(ipSender, Simulator::Now().GetSeconds());
            currentNode->printMeeting(socket->GetNode()->GetId());
            */
        } else if(payload.getType() == HELLO_ACK2){
            currentNode->updatePredictability(pkt, ipSender, ipReceiver);
        }

        // We have complete the exchange of predictability, ready for sent the package propose.
        if (originalPayloadType == HELLO_ACK || originalPayloadType == HELLO_ACK2) {
            std::vector<PayLoadConstructor> bufferPackets = currentNode->getPacketsBuffer();
            for (int buffIndex = 0; buffIndex < (int)bufferPackets.size(); buffIndex++) {
                NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " ho un pacchetto da recapitare a " << bufferPackets[buffIndex].getDestinationAddress());
                currentNode->printPredictability(socket->GetNode()->GetId());
             
                if (bufferPackets[buffIndex].getDestinationAddress() == ipSender) {
                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Payload ricevuto " << stringPayload << " da: " << ipSender);
                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " ehy mbare " << ipSender << " havi du uri ca ti cieccu, c'è posta per te.");
                    bufferPackets[buffIndex].incrementHops();  // Increment the Hops and build a new package content.

                    bufferPackets[buffIndex].setType(PKTREQ);
                    // toString create IP;TTL;UID -> toPacket append also the type of pkt
                    std::ostringstream newcontent = bufferPackets[buffIndex].toString();
                    Ptr<Packet> packet = bufferPackets[buffIndex].toPacketFromString(newcontent);
                    InetSocketAddress remote = InetSocketAddress(ipSender, 80);
                    socket->Connect(remote);
                    socket->Send(packet);  // Send the packet request to the user.

                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Sent PKTREQ to: " << ipSender << " with hops: " << bufferPackets[buffIndex].getHops() << " and uid: " << bufferPackets[buffIndex].getUid());
                    break;
                } else {
                    std::vector<std::string> currentNodePredictability = currentNode->getPredictabilityAsArray();
                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Payload ricevuto " << stringPayload << " da: " << ipSender);
                    for (int currentPredict = 0; currentPredict < (int)currentNodePredictability.size(); currentPredict++) {
                        std::vector<std::string> currentValues = splitString(currentNodePredictability[currentPredict], ":");
                        if (ns3::Ipv4Address(currentValues[0].c_str()) == bufferPackets[buffIndex].getDestinationAddress()) {

                            // Here we have the current predict for current payload (packet) and current node.
                            // Going to search if the other nodes have a great predict
                            std::vector<std::string> dataPayload = value; // Packet already open on top

                            // Skip 1 because at index 1 we have the type payload, -1 because the string end with ;
                            for (int tableIndex = 1; tableIndex < (int)dataPayload.size() - 1; tableIndex++) {
                                std::vector<std::string> tableValues = splitString(dataPayload[tableIndex], ":");
                                // tableValues from pkt payload , currentValue from by Table
                                NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Compare: " << currentValues[0].c_str() << " == " << tableValues[0].c_str() << " == " << bufferPackets[buffIndex].getDestinationAddress() << " | " << atof(currentValues[1].c_str()) << " < " << atof(tableValues[1].c_str()) << " for uid: " << bufferPackets[buffIndex].getUid());
                                if (
                                    ns3::Ipv4Address(currentValues[0].c_str()) == ns3::Ipv4Address(tableValues[0].c_str()) &&
                                    ns3::Ipv4Address(tableValues[0].c_str()) == bufferPackets[buffIndex].getDestinationAddress() &&
                                    atof(currentValues[1].c_str()) < atof(tableValues[1].c_str())) {
                                    bufferPackets[buffIndex].incrementHops();  // Increment the Hops and build a new package content.

                                    bufferPackets[buffIndex].setType(PKTREQ);
                                    // toString create IP;TTL;UID -> toPacket append also the type of pkt
                                    std::ostringstream newcontent = bufferPackets[buffIndex].toString();
                                    Ptr<Packet> packet = bufferPackets[buffIndex].toPacketFromString(newcontent);
                                    InetSocketAddress remote = InetSocketAddress(ipSender, 80);
                                    socket->Connect(remote);
                                    socket->Send(packet);  // Send the packet request to the user.

                                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Sent PKTREQ to: " << ipSender << " with hops: " << bufferPackets[buffIndex].getHops() << " and uid: " << bufferPackets[buffIndex].getUid());
                                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Predict compare: " << atof(currentValues[1].c_str()) << " < " << atof(tableValues[1].c_str()) << " for uid: " << bufferPackets[buffIndex].getUid());
                                    break;
                                }
                            }
                        }
                    }
                }
            
            }
        }

        int MAX_BUFFERSIZE = 5;  // it's temp
        if (payload.getType() == PKTREQ) {
            // std::vector<PayLoadConstructor> getPacketsBuffer()
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Received PKREQ from: " << ipSender << " with Hops: " << payload.getHops() << " and uid: " << payload.getUid());
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Current buffer size: " << currentNode->getPacketsBuffer().size() << " / " << MAX_BUFFERSIZE);
            if ( ((int)currentNode->getPacketsBuffer().size() < MAX_BUFFERSIZE) || (payload.getDestinationAddress() == ipReceiver) ) {
                if((payload.getDestinationAddress() != ipReceiver)){
                    payload.setType(STANDARD);  // Also done in savePacketsInBuffer
                    currentNode->savePacketsInBuffer(payload);
                }else {
                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " aohu mbare il pacchetto è pemméé - from: " << ipSender << " with Hops: " << payload.getHops() << " and uid: " << payload.getUid());
                    if (dataForPackets[payload.getUid()].delivered != true){  // Prevent multiple logs for the same pkg receiver more times
                        dataForPackets[payload.getUid()].delivered = true;
                        dataForPackets[payload.getUid()].delivered_at = Simulator::Now().GetSeconds();
                        dataForPackets[payload.getUid()].hops = payload.getHops();
                    }
                }
                payload.setType(PKTACK);
                std::ostringstream newcontent = payload.toString();
                Ptr<Packet> packet = payload.toPacketFromString(newcontent);
                InetSocketAddress remote = InetSocketAddress(ipSender, 80);
                socket->Connect(remote);
                socket->Send(packet);  // Packet accepted

                NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Sent PKTACK to: " << ipSender << " with hops: " << payload.getHops() << " and uid: " << payload.getUid());
            }
        } else if (payload.getType() == PKTACK) {
            currentNode->increaseBytesSent((double)pkt->GetSize());
            currentNode->increasePacketsSent(1);
            // Remove from buffer array the uid accepted from ack
            std::vector<PayLoadConstructor> bufferPackets = currentNode->getPacketsBuffer();
            for (int buffIndex = 0; buffIndex < (int)bufferPackets.size(); buffIndex++) {
                if (bufferPackets[buffIndex].getUid() == payload.getUid()) {
                    currentNode->removePacketFromBufferByIndex(buffIndex);

                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Received PKTACK from: " << ipSender << " with Hops: " << payload.getHops() << " and uid: " << payload.getUid());
                    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << socket->GetNode()->GetId() << " Remove PKT with uid: " << payload.getUid() << " at index: " << buffIndex);
                    break;
                }
            }
        }
    }
}

static void GeneratePacket(int nodeId, Ipv4Address destinationAddress, uint32_t hops, uint32_t uid) {
    NodeHandler *currentNode = &nodeHandlerArray[nodeId];

    PayLoadConstructor payload = PayLoadConstructor(STANDARD);
    payload.setHops(hops);
    payload.setUid(uid);
    payload.setDestinationAddress(destinationAddress);

    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t" << nodeId << " Create a new Packets for: " << destinationAddress << " with hops: " << hops << " and uid: " << uid);
    if(dataForPackets[uid].start == 0) dataForPackets[uid].start = Simulator::Now().GetSeconds();

    currentNode->savePacketsInBuffer(payload);
}

int main(int argc, char *argv[]) {
    std::string phyMode("DsssRate1Mbps");
    // uint32_t gridWidth = 10;
    // double distance = 150;  // m
    double simulationTime = 4000.00;
    uint32_t seed = 14;
    uint32_t sendAfter = 300;

    uint32_t numPackets = 1;
    uint32_t numNodes = 60;  // by default, 50
    uint32_t sinkNode = 1;
    uint32_t sourceNode = 7;

    uint32_t hops = 0;
    uint32_t UID = 0;

    double rss = -80;  // -dBm

    CommandLine cmd;
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    // cmd.AddValue ("distance", "distance (m)", distance);
    cmd.AddValue("numPackets", "number of packets generated", numPackets);
    cmd.AddValue("numNodes", "number of nodes", numNodes);
    cmd.AddValue("sinkNode", "Receiver node number", sinkNode);
    cmd.AddValue("sourceNode", "Sender node number", sourceNode);
    cmd.AddValue("hops", "hops For each packet", hops);
    cmd.AddValue("seed", "Custom seed for simulation", seed);
    cmd.AddValue("simulationTime", "Set a custom time (s) for simulation", simulationTime);
    cmd.AddValue("sendAfter", "Send the first pkt after", sendAfter);

    cmd.AddValue("rss", "received signal strength", rss);
    cmd.Parse(argc, argv);

    // Fix non-unicast data rate to be the same as that of unicast
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue(phyMode));

    NodeContainer c;
    c.Create(numNodes);

    SeedManager::SetSeed(seed);

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    // FROM WIFI SIMPLE ADHOC GRID

    // set it to zero; otherwise, gain will be added
    wifiPhy.Set("RxGain", DoubleValue(4));
    wifiPhy.Set("TxGain", DoubleValue(4));
    // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("5000.0"),
                                  "Y", StringValue("5000.0"),
                                  "Theta", StringValue("ns3::UniformRandomVariable[Min=-1000.0|Max=1000.0]"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=1000.0|Max=5000.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("15s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10.0]"),  // 36 km/h
                              "Bounds", StringValue("0|10000|0|10000"));

    mobility.InstallAll();

    InternetStackHelper internet;
    internet.Install(c);

    Ipv4AddressHelper ipv4;
    NS_LOG_INFO("Assign IP Addresses.");
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    // InetSocketAddress broadcast = InetSocketAddress(Ipv4Address("255.255.255.255"), 80);

    Ptr<Socket> recvSinkArray[numNodes];
    for (uint32_t i = 0; i < numNodes; ++i) {
        nodeHandlerArray.push_back(*new NodeHandler(c.Get(i)->GetId()));
        recvSinkArray[i] = Socket::CreateSocket(c.Get(i), tid);
        recvSinkArray[i]->Bind(local);
        recvSinkArray[i]->SetRecvCallback(MakeCallback(&ReceivePacket));

        // recvSinkArray[i]->SetAllowBroadcast(true);
        // recvSinkArray[i]->Connect(broadcast);

        Simulator::Schedule(Seconds(1 * i), &GenerateHello,
                            recvSinkArray[i]);
    }

    Ipv4InterfaceAddress iaddr = c.Get(sinkNode)->GetObject<Ipv4>()->GetAddress(1, 0);
    Ipv4Address destinationAddress = iaddr.GetLocal();

    for (uint32_t i = 0; i < numPackets; i++) {
        PacketLogData dataPacket = {false, 0.00, 0.00, 0};
        dataForPackets.push_back(dataPacket);
        Simulator::Schedule(Seconds(sendAfter + (10 * i)), &GeneratePacket,
                            c.Get(sourceNode)->GetId(), destinationAddress, hops, UID);

        UID += 1;
    }

    AnimationInterface anim("prophet-anim.xml");
    anim.SetMaxPktsPerTraceFile(500000);

    anim.UpdateNodeDescription(c.Get(sourceNode), "Sender");
    anim.UpdateNodeDescription(c.Get(sinkNode), "Receiver");

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();
    Simulator::Destroy();

    int deliveredCounter = 0;
    for (int i = 0; i < (int)dataForPackets.size(); i++) {
        if (dataForPackets[i].delivered == true) {
            deliveredCounter++;
            NS_LOG_UNCOND("- Packets " << i + 1 << " delta delivery: \t" << (double)(dataForPackets[i].delivered_at - dataForPackets[i].start));
        }
        else NS_LOG_UNCOND("- Packets " << i + 1 << " delta delivery: \t" << 0);
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
    for (uint32_t i = 0; i < numNodes; ++i) {
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