#!/bin/bash

# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

now=$(date --utc +%Y%m%dT%H%M%S)
cpucount=$(grep -c ^processor /proc/cpuinfo)
exe="$(dirname "$0")/../benchmarks"

function benchmark() {
    lb="/usr/bin/stdbuf -oL"
    ((N=33))
    for((i=1;i<=N;++i)); do
        $lb echo -n "[$i/$N] "
        sudo chrt -f 50 "$exe"
    done
}

sudo hugeadm --pool-pages-min 1GB:1 --pool-pages-max 1GB:1
sudo cpupower frequency-set --related --governor performance >/dev/null

benchmark | tee results-${cpucount}.${now}.txt

$(dirname "$0")/benchmark-prologue.sh
sudo cpupower frequency-set --related --governor powersave >/dev/null
$(dirname "$0")/benchmark-epilogue.sh
