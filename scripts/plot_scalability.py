#!/usr/bin/env python

# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

import sys
import math
import numpy as np
import pandas as pd
from scipy import stats
from pprint import pprint

import matplotlib.pyplot as plt
import matplotlib
from matplotlib.ticker import MultipleLocator, FuncFormatter

from matplotlib import rcParams
rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Ubuntu']

from parse_output import *

# print("numpy", np.__version__)
# print("pandas", pd.__version__)
# print("matplotlib", matplotlib.__version__)

results = list(parse_output(sys.stdin))
# pprint(results)

def plot_scalability(results):
    df = as_scalability_df(results)
    # print(df.to_json(orient='columns'))
    # print(df.columns)

    style = {
        'pthread_spinlock': 's-',
        'boost::lockfree::queue': 's-',

        'tbb::concurrent_bounded_queue': 's-',
        'tbb::spin_mutex': 's-',
        'tbb::speculative_spin_mutex': 's-',

        'AtomicQueue': 'x-r',
        'AtomicQueue2': 'x-y',

        'BlockingAtomicQueue': 'o-r',
        'BlockingAtomicQueue2': 'o-y',
        }
    ax = df.plot(title='Scalability, Intel Xeon Gold 6132', style=style)
    # ax = df.plot(title='Scalability, Intel Core i7-7700K 5GHz', style=style)
    ax.autoscale(tight=False)
    ax.legend(frameon=False)
    ax.get_yaxis().set_major_formatter(FuncFormatter(lambda x, p: format(int(x), ',')))
    ax.set_ylabel('msg/sec')
    ax.set_ylim(top=12e7, bottom=-1e6)
    # ax.yaxis.set_major_locator(MultipleLocator(1e9))
    ax.set_xlabel('number of producers, number of consumers')
    ax.yaxis.set_major_locator(MultipleLocator(1e7))
    ax.xaxis.set_major_locator(MultipleLocator(1))
    ax.set_frame_on(False)
    ax.grid()
    plt.show()


plot_scalability(results)
