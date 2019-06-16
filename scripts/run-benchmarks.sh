#!/bin/bash

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

benchmark | tee results-${cpucount}.${now}.txt
