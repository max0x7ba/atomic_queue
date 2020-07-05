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

output = defaultdict(list) # name: thread, min, max, mean, stdev
for (name, threads), data in df.groupby(['queue', 'threads']):
    s = data["msg/sec"].describe(percentiles=None)
    threads = int(threads)
    output[name].append([threads, *[int(s[f]) for f in ['min', 'max', 'mean', 'std']]])
json.dump(output, sys.stdout)
print()
