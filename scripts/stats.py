#!/usr/bin/env python

# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

import sys
import re
import math
import csv
import numpy as np
from scipy import stats
from pprint import pprint
from collections import defaultdict

results=defaultdict(lambda: defaultdict(list))

r = re.compile("\s*(.+):\s+([,.0-9]+)\s+(\S+)")
for line in sys.stdin:
    m = r.match(line)
    if m:
        results[m.group(1)][m.group(3)].append(float(m.group(2).replace(',', '')))

def format_msg_sec(d, media, benchmark):
    return "{:11,.0f} {} (median: {:11,.0f}, mean: {:11,.0f} stdev: {:11,.0f})".format(d.minmax[1], benchmark, median, d.mean, math.sqrt(d.variance))

def format_round_trip(d, media, benchmark):
    return "{:11.9f} {} (median: {:11,.0f}, mean: {:11.9f} stdev: {:11.9f})".format(d.minmax[0], benchmark, median, d.mean, math.sqrt(d.variance))

fmt = {
    'msg/sec': format_msg_sec,
    'sec/round-trip': format_round_trip
    }

csv_file = csv.writer(open("results.csv", "w"), csv.excel_tab)
csv_file.writerow(["name", "min", "max", "mean", "stdev"])
for benchmark in ['msg/sec', 'sec/round-trip']:
    queues = sorted(results.keys())
    for queue in queues:
        qr = results[queue]
        runs = qr.get(benchmark, None)
        if not runs:
            continue
        d = stats.describe(runs)
        median = np.median(runs)
        desc = fmt[benchmark](d, median, benchmark)
        print("{:>40s}: {}".format(queue, desc))
        csv_file.writerow([queue, d.minmax[0], d.minmax[1], d.mean, math.sqrt(d.variance)])
