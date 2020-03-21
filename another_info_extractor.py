#NB: put LOG_LEVEL TO "EXTRACTOR"
import copy

i_packet = {
	"send_at": 0.0,
	"uid": 0,
	"ack":False,
	"received": False,
	"flying_pkt":0,
	"until_now_pkt":0
}

'''
NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT SENT, UID:    " << UID)
NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT ACK SENT, UID:    " << UID)
NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT RECEIVED, UID:    " << UID)
NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT DESTINATION REACHED, UID:    " << UID)
'''
pkt_list = []
f = open('terminal.txt')
line = f.readline()
while line:
	if "PKT SENT, UID:" in line or "PKT ACK SENT, UID:" in line:
		temp_pkt = copy.deepcopy(i_packet)

		temp_pkt["send_at"] = double((line.split("s"))[0])
		temp_pkt["uid"] = int((line.split("    "))[1])
		temp_pkt["ack"] = "ACK" in line
		if len(pkt_list)-1 < temp_pkt["uid"]:
			pkt_list.append([])

		if len(pkt_list[temp_pkt["uid"]])>0:
			temp_pkt["until_now_pkt"] = pkt_list[temp_pkt["uid"]][-1]["until_now_pkt"]
		
		pkt_list[temp_pkt["uid"]].append(temp_pkt)

	if "PKT RECEIVED, UID" in line:
		uid = int((line.split("    "))[1])
		
		pkt_list[uid][-1]["until_now_pkt"] += 1
		pkt_list[uid][-1]["flying_pkt"] += 1

	if "PKT DESTINATION REACHED, UID:" in line:
		uid = int((line.split("    ")[1]))

		temp_pkt = copy.deepcopy(i_packet)
		temp_pkt["send_at"] = double((line.split("s"))[0])
		temp_pkt["until_now_pkt"] = pkt_list[uid][-1]["until_now_pkt"] + 1
		temp_pkt["flying_pkt"] = pkt_list[uid][-1]["flying_pkt"] + 1
		temp_pkt["received"] = True

		pkt_list[uid].append(temp_pkt)

	line = f.readline()
f.close()

print("SEND_AT;UID;RECEIVED;FLYING_PACKET;PACKET_UNTIL_NOW\n")
for i in range(len(pkt_list)):
	for j in range(len(pkt_list[i])):
		print("{};{};{};{};{}\n".format(
			pkt_list[i][j]["send_at"],
			pkt_list[i][j]["uid"],
			pkt_list[i][j]["received"],
			pkt_list[i][j]["flying_pkt"],
			pkt_list[i][j]["until_now_pkt"]
		))


