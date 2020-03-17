#!/usr/bin/python3
# -*- coding: utf-8 -*-

import os
import logging
import datetime
import time
import subprocess
import matplotlib.pyplot as plt

from multiprocessing import Process

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
    "- Total PacketsReceived:"
]

def start_simulation(cmd, n, output):
    logger.info("{}) START - {}.".format(str(n).zfill(2), cmd))
    checkpoint = datetime.datetime.now()
    fullcmd = './waf --run "{}" > {} 2>&1'.format(cmd, output)
    time.sleep(1)
    p = subprocess.Popen(fullcmd, cwd=NS3PATH, shell=True)
    p.wait()
    time.sleep(2)
    elpased_s = (datetime.datetime.now() - checkpoint).total_seconds()
    logger.info("{}) END - {}. Time elapse: {}m".format(str(n).zfill(2), cmd, round(elpased_s / 60, 2)))


if __name__ == "__main__":
    commands_list = [
        'scratch/prophet --numNodes=15 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/prophet --numNodes=20 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/prophet --numNodes=30 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/prophet --numNodes=40 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/prophet --numNodes=50 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/prophet --numNodes=70 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/prophet --numNodes=90 --numPackets=5 --sinkNode=14 --sourceNode=1',

        'scratch/wifi-simple-adhoc-grid-anim --numNodes=15 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/wifi-simple-adhoc-grid-anim --numNodes=20 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/wifi-simple-adhoc-grid-anim --numNodes=30 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/wifi-simple-adhoc-grid-anim --numNodes=40 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/wifi-simple-adhoc-grid-anim --numNodes=50 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/wifi-simple-adhoc-grid-anim --numNodes=70 --numPackets=5 --sinkNode=14 --sourceNode=1',
        'scratch/wifi-simple-adhoc-grid-anim --numNodes=90 --numPackets=5 --sinkNode=14 --sourceNode=1'

        # 'scratch/wifi-simple-adhoc-grid-anim --numNodes=50 --ttl=4 --sinkNode=14 --sourceNode=1'
        # 'scratch/wifi-simple-adhoc-grid-anim --numNodes=50 --ttl=5 --sinkNode=14 --sourceNode=1'
        # 'scratch/wifi-simple-adhoc-grid-anim --numNodes=50 --ttl=6 --sinkNode=14 --sourceNode=1'
        # 'scratch/wifi-simple-adhoc-grid-anim --numNodes=50 --ttl=7 --sinkNode=14 --sourceNode=1'
        # 'scratch/wifi-simple-adhoc-grid-anim --numNodes=50 --ttl=8 --sinkNode=14 --sourceNode=1'
        # 'scratch/wifi-simple-adhoc-grid-anim --numNodes=50 --ttl=9 --sinkNode=14 --sourceNode=1'
    ]
    outputs = []

    if not os.path.exists(CURRENT_PATH + "/aio-simulator"):
        os.mkdir(CURRENT_PATH + "/aio-simulator")

    output_folder = CURRENT_PATH + "/aio-simulator/" + datetime.datetime.now().strftime("%d%m%Y%H%M%S")
    os.mkdir(output_folder)

    multiprocess = []
    for index in range(0, len(commands_list)):
        fulloutput = output_folder + "/{}.out.txt".format(commands_list[index].replace("scratch/", "").replace(" ", "\ ").strip())
        outputs.append(fulloutput)
        multiprocess.append(Process(target=start_simulation, args=(commands_list[index], index+1, fulloutput)))
        multiprocess[len(multiprocess) - 1].start()
        time.sleep(5)

    for index in range(0, len(multiprocess)):
        multiprocess[index].join()

    dataforplot = {}
    for fname in outputs:
        fname = fname.replace("\ ", " ")
        with open(fname, "r") as f:
            outlines = f.readlines()
        outlines = outlines[-15:]

        cmd = fname.split('/')[-1]
        args = cmd.split(' ')
        # print(cmd, args)

        alghname = args[0]
        if alghname not in dataforplot:
            dataforplot[alghname] = {}

        nnodes = args[1].replace("--numNodes=", "")

        for match_string in FINAL_REPORT:
            for single_line in outlines:
                if single_line.startswith(match_string):
                    value = single_line.replace(match_string, '').strip()
                    studycase = match_string.replace('-', '').replace(':', '').strip()
                    if studycase not in dataforplot[alghname]:
                        dataforplot[alghname][studycase] = []
                    dataforplot[alghname][studycase].append({
                        "x": float(nnodes.strip()),
                        "y": float(value.replace('%', '').strip())
                    })
                    # print("{} \t {}".format(alghname, match_string, value, nnodes))
                    # print("{} \t {}".format(value, nnodes))
                    break


    for alghname in dataforplot:
        for studycase in dataforplot[alghname]:
            plt.plot([item['x'] for item in dataforplot[alghname][studycase]], [item['y'] for item in dataforplot[alghname][studycase]])
            plt.xlabel('Node number'.upper())
            plt.ylabel(studycase.upper())
            plt.suptitle(alghname.upper())
            plt.show()

