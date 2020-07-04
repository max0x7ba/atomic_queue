#!/usr/bin/env python

# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

import sys
import pandas as pd
import json
from collections import defaultdict
from pprint import pprint

from parse_output import *

results = list(parse_output(sys.stdin))
df = as_scalability_df(results)

columns = defaultdict(list) # name: [threads, mean].
errorbars = defaultdict(list) # name: [threads, low, high].
for (name, threads), data in df.groupby(['queue', 'threads']):
    s = data["msg/sec"].describe(percentiles=None)
    threads = int(threads)
    columns[name].append([threads, int(s["mean"])])
    errorbars[name].append([threads, int(s["min"]), int(s["max"])])

output1 = [{"name" : n, "type": "column", "data": d} for n, d in columns.items()]
output2 = [{"name" : n, "type": "errorbar", "data": d} for n, d in errorbars.items()]
json.dump([j for i in zip(output1, output2) for j in i], sys.stdout)
print()
