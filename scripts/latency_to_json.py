#!/usr/bin/env python

# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

import sys
import pandas as pd
from pprint import pprint

from parse_output import *

results = list(parse_output(sys.stdin))
df = as_latency_df(results)
df = df.groupby('queue').min()
pprint(df)
df["sec/round-trip"].to_json(sys.stdout, orient='columns')
print()
