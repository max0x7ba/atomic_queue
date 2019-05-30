#!/usr/bin/env python

import re
import pandas as pd

_parser = re.compile("\s*(\S+):\s+([,.0-9]+)\s+(\S+)")

def parse_output(f):
    for line in f:
        m = _parser.match(line)
        if m:
            queue = m.group(1)
            units = m.group(3)
            value = float(m.group(2).replace(',', ''))
            yield queue, units, value


def extract_name_threads(name_theads):
    name, threads = name_theads.split(',')
    return name, int(threads)


def as_scalability_df(results):
    df = pd.DataFrame.from_records(((*extract_name_threads(r[0]), r[2]) for r in results), columns=['queue', 'threads', 'msg/sec'])
    df = df.groupby(['queue', 'threads']).max().unstack(level=0).droplevel(0, axis=1)
    return df
