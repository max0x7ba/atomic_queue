#!/bin/bash

set +e # Ignore failures.

sudo hugeadm --pool-pages-min 1GB:1 --pool-pages-max 1GB:1
sudo cpupower frequency-set --related --governor performance >/dev/null

if [[ -e /proc/sys/kernel/sched_rt_runtime_us ]]; then
    if [[ ! -e .sched_rt_runtime_us.txt ]]; then
        cat /proc/sys/kernel/sched_rt_runtime_us >.sched_rt_runtime_us.txt~
        mv -f .sched_rt_runtime_us.txt{~,}
    fi
    echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us >/dev/null
fi
