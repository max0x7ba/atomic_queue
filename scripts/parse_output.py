#!/usr/bin/env python

import re

parser = re.compile("\s*(\S+):\s+([,.0-9]+)\s+(\S+)")

def parse_output(f):
    for line in f:
        m = parser.match(line)
        if m:
            queue = m.group(1)
            units = m.group(3)
            value = float(m.group(2).replace(',', ''))
            yield queue, units, value


def extract_name_threads(name_theads):
    name, threads = name_theads.split(',')
    return name, int(threads)
