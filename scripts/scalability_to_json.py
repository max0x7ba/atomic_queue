#!/usr/bin/env python

import sys
import pandas as pd
from pprint import pprint

from parse_output import *

results = list(parse_output(sys.stdin))
df = as_scalability_df(results)
df.to_json(sys.stdout, orient='columns')
print()
