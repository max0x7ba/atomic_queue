#!/bin/bash

function prologue {
    set +e # Ignore failures.

    sudo hugeadm --pool-pages-min 1GB:1
    sudo cpupower frequency-set --related --governor performance >/dev/null

    if [[ -e /proc/sys/kernel/sched_rt_runtime_us ]]; then
        if [[ ! -e .sched_rt_runtime_us.txt ]]; then
            cat /proc/sys/kernel/sched_rt_runtime_us >.sched_rt_runtime_us.txt~
            mv -f .sched_rt_runtime_us.txt{~,}
        fi
        echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us >/dev/null
    fi
}

function epilogue {
    set +e # Ignore failures.
    sudo cpupower frequency-set --related --governor powersave >/dev/null
    [[ -f .sched_rt_runtime_us.txt ]] && sudo tee /proc/sys/kernel/sched_rt_runtime_us >/dev/null <.sched_rt_runtime_us.txt
}

function benchmark {
    trap "epilogue" EXIT
    prologue
    (set -e -x; exec "$@")
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    benchmark "$@"
fi
