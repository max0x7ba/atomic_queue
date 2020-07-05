#!/usr/bin/env python

# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

import sys
import pandas as pd
import json
from pprint import pprint

from parse_output import *

results = list(parse_output(sys.stdin))
df = as_latency_df(results)
output = dict() # name: min, max, mean, stdev
for name, data in df.groupby('queue'):
    s = data["sec/round-trip"].describe()
    output[name] = [int(s[f] * 1e9) for f in ['min', 'max', 'mean', 'std']]
json.dump(output, sys.stdout)
print()
