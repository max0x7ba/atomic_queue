#!/bin/bash

# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

lb="/usr/bin/stdbuf --output=L"

minutes=${1:-10}
$lb echo "Running benchmarks for ${minutes} minutes."
end_time=$(($(date +%s) + minutes * 60))

now=$(date --utc +%Y%m%dT%H%M%S)
cpucount=$(grep -c ^processor /proc/cpuinfo)
exe="$(dirname "$0")/../benchmarks"

function benchmark() {
    i=0
    while((1)); do
        remaining=$((end_time - $(date +%s)))
        ((remaining <= 0)) && break
        ((++i))
        $lb echo "[$i] $((remaining / 60))m:$((remaining % 60))s to go..."
        sudo chrt -f 50 "$exe"
    done
}

$(dirname "$0")/benchmark-prologue.sh
benchmark | tee results-${cpucount}.${now}.txt
$(dirname "$0")/benchmark-epilogue.sh
