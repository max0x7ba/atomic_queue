#!/bin/bash

set +e # Ignore failures.

sudo hugeadm --pool-pages-min 1GB:1 --pool-pages-max 1GB:1
sudo cpupower frequency-set --related --governor performance >/dev/null
~/scripts/cpu-fans.sh 5 >/dev/null
