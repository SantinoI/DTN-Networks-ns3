#!/usr/bin/python3
# -*- coding: utf-8 -*-
import copy
import os
import logging
import datetime
import time
import subprocess
import multiprocessing
import matplotlib.pyplot as plt

logging.basicConfig(
    format="%(asctime)s - %(levelname)s - [%(funcName)s]: %(message)s", datefmt="%d/%m %H:%M:%S", level=logging.INFO,
)
logger = logging.getLogger("AIO-SIMULATOR")

"""
fh = logging.FileHandler("aio-simulator_{}.log".format(datetime.datetime.now().strftime("%Y-%m-%d_%H:%M:%S")), "a",)
fh.setLevel(logging.INFO)
formatter = logging.Formatter("%(asctime)s - %(levelname)s - [%(funcName)s]: %(message)s")
fh.setFormatter(formatter)
logger.addHandler(fh)
"""

NS3PATH = "/home/alessandro/Documents/ns-allinone-3.30.1/ns-3.30.1/"
CURRENT_PATH = os.path.abspath(os.getcwd())

FINAL_REPORT = [
    "- Packets sent:",
    "- Packets delivered:",
    "- Delivery percentage:",
    "- Total BytesSent:",
    "- Total BytesReceived:",
    "- Total PacketsSent:",
    "- Total PacketsReceived:",
    "- Total BytesHelloSent:",
    "- Total BytesHelloReceived:",
    "- Total PacketsHelloSent:",
    "- Total PacketsHelloReceived:",
]

i_packet = {"send_at": 0.0, "uid": 0, "ack": False, "received": False, "flying_pkt": 0, "until_now_pkt": 0}


def update_dataforplot(dataforplot, single_line, match_string, alghname, nnodes):
    if alghname not in dataforplot:
        dataforplot[alghname] = {}
    value = single_line.replace(match_string, "").strip()
    studycase = match_string.replace("-", "").replace(":", "").strip()
    if studycase not in dataforplot[alghname]:
        dataforplot[alghname][studycase] = []
    dataforplot[alghname][studycase].append({"x": float(nnodes.strip()), "y": float(value.replace("%", "").strip())})
    return dataforplot


def another_info_extractor(lines, file_name):
    """
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT SENT, UID:    " << UID)
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT ACK SENT, UID:    " << UID)
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT RECEIVED, UID:    " << UID)
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s\t PKT DESTINATION REACHED, UID:    " << UID)
    """
    pkt_list = []
    for line in lines:
        if "PKT SENT, UID:" in line or "PKT ACK SENT, UID:" in line:
            temp_pkt = copy.deepcopy(i_packet)

            temp_pkt["send_at"] = float((line.split("s"))[0])
            temp_pkt["uid"] = int((line.split("    "))[1])
            temp_pkt["ack"] = "ACK" in line
            if len(pkt_list) - 1 < temp_pkt["uid"]:
                pkt_list.append([])

            if len(pkt_list[temp_pkt["uid"]]) > 0:
                temp_pkt["until_now_pkt"] = pkt_list[temp_pkt["uid"]][-1]["until_now_pkt"]

            pkt_list[temp_pkt["uid"]].append(temp_pkt)

        if "PKT RECEIVED, UID" in line:
            uid = int((line.split("    "))[1])

            pkt_list[uid][-1]["until_now_pkt"] += 1
            pkt_list[uid][-1]["flying_pkt"] += 1

        if "PKT DESTINATION REACHED, UID:" in line:
            uid = int((line.split("    ")[1]))

            temp_pkt = copy.deepcopy(i_packet)
            temp_pkt["send_at"] = float((line.split("s"))[0])
            temp_pkt["until_now_pkt"] = pkt_list[uid][-1]["until_now_pkt"] + 1
            temp_pkt["flying_pkt"] = pkt_list[uid][-1]["flying_pkt"] + 1
            temp_pkt["received"] = True

            pkt_list[uid].append(temp_pkt)

    output_csv = open("{}.csv".format(file_name), "w")
    output_csv.write("SEND_AT;UID;RECEIVED;FLYING_PACKET;PACKET_UNTIL_NOW\n")

    for i in range(len(pkt_list)):
        for j in range(len(pkt_list[i])):
            output_csv.write(
                "{};{};{};{};{}\n".format(
                    pkt_list[i][j]["send_at"],
                    pkt_list[i][j]["uid"],
                    pkt_list[i][j]["received"],
                    pkt_list[i][j]["flying_pkt"],
                    pkt_list[i][j]["until_now_pkt"],
                )
            )

    output_csv.close()


def start_simulation(cmd, n, output):
    logger.info("{}) RUN - {}.".format(str(n).zfill(2), cmd))
    checkpoint = datetime.datetime.now()
    fullcmd = './waf --run "{}" > {} 2>&1'.format(cmd, output)
    time.sleep(1)
    p = subprocess.Popen(fullcmd, cwd=NS3PATH, shell=True)
    p.wait()
    time.sleep(2)
    elpased_s = (datetime.datetime.now() - checkpoint).total_seconds()
    logger.info("{}) END - {}. Time elapse: {}m".format(str(n).zfill(2), cmd, round(elpased_s / 60, 2)))


if __name__ == "__main__":
    default_args = "--sinkNode=14 --sourceNode=0 --seed=112 --simulationTime=5000 --sendAfter=100"
    alghoritms_fname = ["prophet", "epidemic"]
    num_packets = 3
    num_nodes_chunk = [15, 20, 30, 40, 50, 70, 90, 110] # , 130, 150, 200, 250, 500]
    num_nodes_chunk.reverse()
    commands_list = []
    for alghname in alghoritms_fname:
        for nnodes in num_nodes_chunk:
            commands_list.append("scratch/{} --numNodes={} --numPackets={} {}".format(alghname, nnodes, num_packets, default_args))

    logger.info("Ready for start {} commands: {}".format(len(commands_list), "\n» " + "\n» ".join(commands_list)))
    time.sleep(5)
    outputs = []

    if not os.path.exists(CURRENT_PATH + "/aio-simulator"):
        os.mkdir(CURRENT_PATH + "/aio-simulator")

    output_folder = CURRENT_PATH + "/aio-simulator/" + datetime.datetime.now().strftime("%d%m%Y%H%M%S")
    os.mkdir(output_folder)

    max_process = int(multiprocessing.cpu_count() / 2)
    multiprocess = []
    for index in range(0, len(commands_list)):
        fulloutput = output_folder + "/{}.out.txt".format(
            commands_list[index].replace("scratch/", "").replace(default_args, "").strip().replace(" ", "\ ").strip()
        )
        outputs.append(fulloutput)
        multiprocess.append(multiprocessing.Process(target=start_simulation, args=(commands_list[index], index + 1, fulloutput)))
        multiprocess[len(multiprocess) - 1].start()

        if len(multiprocess) == max_process:
            multiprocess[0].join()
            multiprocess.pop(0)

    for index in range(0, len(multiprocess)):
        multiprocess[index].join()

    dataforplot = {}
    for fname in outputs:
        fname = fname.replace("\ ", " ").replace("\\", "")
        with open(fname, "r") as f:
            outlines = f.readlines()

        another_info_extractor(lines=outlines, file_name=fname)

        outlines = outlines[-25:]  # ESAGERO

        cmd = fname.split("/")[-1]
        args = cmd.split(" ")
        # print(cmd, args)

        alghname = args[0]
        nnodes = args[1].replace("--numNodes=", "")

        # - Packets {} delta delivery:
        for match_string in FINAL_REPORT:
            for single_line in outlines:
                if single_line.startswith(match_string):
                    dataforplot = update_dataforplot(dataforplot, single_line, match_string, alghname, nnodes)
                    break

        for packet_string in ["- Packets {} delta delivery:", "- Packets {} TTL/HOPS:"]:
            npkts = args[1].replace("--numPackets=", "")
            for npkt in range(1, len(npkts) + 1):
                founded = False
                for single_line in outlines:
                    match_string = packet_string.format(npkt)
                    if single_line.startswith(match_string):
                        dataforplot = update_dataforplot(dataforplot, single_line, match_string, alghname, nnodes)
                        founded = True
                        break

    for alghname in dataforplot:
        for studycase in dataforplot[alghname]:
            data_x = [item["x"] for item in dataforplot[alghname][studycase]]
            data_y = [item["y"] for item in dataforplot[alghname][studycase]]
            plt.plot(data_x, data_y)
            xlabel = "Node number".upper()
            plt.xlabel(xlabel)
            studycase_str = studycase.upper().replace("-", "").replace(":", "")
            plt.ylabel(studycase_str)
            plt.suptitle(alghname.upper())
            logger.info("{} - {} - {} , {}".format(alghname, studycase_str, data_x, data_y))
            out_fname = "{}/{} - {}".format(output_folder, alghname, studycase_str)
            with open("{}.xy.csv".format(out_fname), "w+") as f:
                f.write("{};{}\n".format(xlabel, studycase_str))
                for index in range(0, len(data_x)):
                    f.write("{};{}".format(data_x[index], data_y[index]) + ("\n" if index != len(data_x) - 1 else ""))
            plt.savefig("{}.png".format(out_fname))
            # plt.savefig('{}.pdf'.format(out_fname))
            plt.show()
