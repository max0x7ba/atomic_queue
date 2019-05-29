#!/usr/bin/env python

import sys
import re
import math
import numpy as np
import pandas as pd
from scipy import stats
from pprint import pprint

import matplotlib.pyplot as plt
import matplotlib

from matplotlib import rcParams
rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Ubuntu']

print("numpy", np.__version__)
print("pandas", pd.__version__)
print("matplotlib", matplotlib.__version__)

def parse_output(f):
    parser = re.compile("\s*(\S+):\s+([,.0-9]+)\s+(\S+)")
    for line in f:
        m = parser.match(line)
        if m:
            queue = m.group(1)
            units = m.group(3)
            value = float(m.group(2).replace(',', ''))
            yield queue, units, value

results = list(parse_output(sys.stdin))
# pprint(results)

def extract_name_threads(name_theads):
    name, threads = name_theads.split(',')
    return name, int(threads)

def plot_scalability(results):
    df = pd.DataFrame.from_records(((*extract_name_threads(r[0]), r[2]) for r in results), columns=['queue', 'threads', 'msg/sec'])
    df = df.groupby(['queue', 'threads']).max().unstack(level=0).droplevel(0, axis=1)
    print(df)
    print(df.columns)

    style = {
        'pthread_spinlock': 's-k',
        'boost::lockfree::queue': 's-g',
        'tbb::concurrent_bounded_queue': 's-b',

        'AtomicQueue': 'x-r',
        'AtomicQueue2': 'x-y',

        'BlockingAtomicQueue': 'o-r',
        'BlockingAtomicQueue2': 'o-y',
        }
    ax = df.plot(title='Scalability', style=style)
    ax.autoscale(tight=False)
    ax.legend(frameon=False)
    ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))
    ax.set_ylabel('msg/sec')
    ax.set_xlabel('threads')
    ax.set_frame_on(False)
    ax.grid()
    plt.show()


plot_scalability(results)
