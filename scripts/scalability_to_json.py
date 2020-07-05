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

# name
## thread, min, max, mean, stdev
output3 = defaultdict(list)
for (name, threads), data in df.groupby(['queue', 'threads']):
    s = data["msg/sec"].describe(percentiles=None)
    threads = int(threads)
    output3[name].append([threads, *[int(s[f]) for f in ['min', 'max', 'mean', 'std']]])
json.dump(output3, sys.stdout)
print()
