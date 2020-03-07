#NB: put LOG_LEVEL TO "NORMAL"
import copy

i_packet = {
	"send_at": 0.0,
	"uid": 0,
	"received": False,
	"flying_pkt":0,
	"until_now_pkt":0
}
#0s	10.1.0.45    44    Going to send packet with uid:    0    and TTL:    6
#0.00110674s	10.1.0.80    79    Received pkt size: 14 bytes with uid    0    and TTL    5    from: 10.1.0.45 to: 10.1.0.46
#0.818595s I am 10.1.0.46 finally received the package with uid:    0
pkt_list = []
f = open('terminal.txt')
line = f.readline()
while line:
	if "Going to send packet" in line:
		send_at = (line.split("s"))[0]
		line = line.split("    ")
		uid = int(line[3])
		
		if len(pkt_list)-1 < uid:
			pkt_list.append([])

		temp_pkt = copy.deepcopy(i_packet)
		
		temp_pkt["uid"] = uid
		temp_pkt["send_at"] = send_at

		if len(pkt_list[uid])>0:
			temp_pkt["until_now_pkt"] = pkt_list[uid][-1]["until_now_pkt"]
		
		pkt_list[uid].append(temp_pkt)

	if "Received pkt size" in line:
		
		line = line.split("    ")
		uid = int(line[3])
		
		pkt_list[uid][-1]["until_now_pkt"] += 1
		pkt_list[uid][-1]["flying_pkt"] += 1

	if "finally received the package" in line:
		send_at = (line.split("s"))[0]
		line = line.split("    ")
		uid = int(line[1])

		temp_pkt = copy.deepcopy(i_packet)
		
		temp_pkt["until_now_pkt"] = pkt_list[uid][-1]["until_now_pkt"] + 1
		temp_pkt["flying_pkt"] = pkt_list[uid][-1]["flying_pkt"] + 1
		temp_pkt["received"] = True
		temp_pkt["send_at"] = send_at

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


