#!/usr/bin/python3
# -*- coding: utf-8 -*-

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
    # "- Packets sent:",
    "- Packets delivered:",
    "- Delivery percentage:",
    # "- Total BytesSent:",
    # "- Total BytesReceived:",
    # "- Total PacketsSent:",
    # "- Total PacketsReceived:",
]


def update_dataforplot(dataforplot, single_line, match_string, alghname, nnodes):
    if alghname not in dataforplot:
        dataforplot[alghname] = {}
    value = single_line.replace(match_string, "").strip()
    studycase = match_string.replace("-", "").replace(":", "").strip()
    if studycase not in dataforplot[alghname]:
        dataforplot[alghname][studycase] = []
    dataforplot[alghname][studycase].append({"x": float(nnodes.strip()), "y": float(value.replace("%", "").strip())})
    return dataforplot


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
    default_args = "--sinkNode=14 --sourceNode=0 --seed=114 --simulationTime=1000 --sendAfter=250"
    alghoritms_fname = ["prophet", "wifi-simple-adhoc-grid-anim"]
    num_packets = 3
    num_nodes_chunk = [15, 20, 30, 40, 50, 70, 90, 110, 130, 150, 200, 250, 500]
    commands_list = []
    for alghname in alghoritms_fname:
        for nnodes in num_nodes_chunk:
            commands_list.append("scratch/{} --numNodes={} --numPackets={} {}".format(alghname, nnodes, num_packets, default_args))

    logger.info("Ready for start {} commands: {}".format(len(commands_list), "\n» " + "\n» ".join(commands_list)))
    time.sleep(15)
    outputs = []

    if not os.path.exists(CURRENT_PATH + "/aio-simulator"):
        os.mkdir(CURRENT_PATH + "/aio-simulator")

    output_folder = CURRENT_PATH + "/aio-simulator/" + datetime.datetime.now().strftime("%d%m%Y%H%M%S")
    os.mkdir(output_folder)

    max_process = int(multiprocessing.cpu_count() / 2)
    multiprocess = []
    for index in range(0, len(commands_list)):
        fulloutput = output_folder + "/{}.out.txt".format(
            commands_list[index].replace("scratch/", "").replace(default_args, "").replace(" ", "\ ").strip()
        )
        outputs.append(fulloutput)
        multiprocess.append(multiprocessing.Process(target=start_simulation, args=(commands_list[index], index + 1, fulloutput)))
        multiprocess[len(multiprocess) - 1].start()

        if len(multiprocess) == max_process:
            multiprocess[0].join()
            multiprocess.pop(0)

    dataforplot = {}
    for fname in outputs:
        fname = fname.replace("\ ", " ").replace("\\", "")
        with open(fname, "r") as f:
            outlines = f.readlines()
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

        npkts = args[1].replace("--numPackets=", "")
        for npkt in range(1, len(npkts)+1):
            founded = False
            for single_line in outlines:
                match_string = "- Packets {} delta delivery:".format(npkt)
                if single_line.startswith(match_string):
                    dataforplot = update_dataforplot(dataforplot, single_line, match_string, alghname, nnodes)
                    founded = True
                    break

    for alghname in dataforplot:
        for studycase in dataforplot[alghname]:
            data_x = [item["x"] for item in dataforplot[alghname][studycase]]
            data_y = [item["y"] for item in dataforplot[alghname][studycase]]
            plt.plot(data_x, data_y)
            plt.xlabel("Node number".upper())
            studycase_str = studycase.upper().replace('-', '').replace(':', '')
            plt.ylabel(studycase_str)
            plt.suptitle(alghname.upper())
            logger.info("{} - {} - {} , {}".format(alghname, studycase_str, data_x, data_y))
            plt_fname = "{}/{} - {}".format(output_folder, alghname, studycase_str)
            plt.savefig('{}.png'.format(plt_fname))
            # plt.savefig('{}.pdf'.format(plt_fname))
            plt.show()