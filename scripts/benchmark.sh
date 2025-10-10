#!/bin/bash

set -e # Exit on errors.

sched_rt=/proc/sys/kernel/sched_rt_runtime_us
orig_sched_rt=/tmp/${sched_rt##*/}.txt

function prologue {
    sudo hugeadm --pool-pages-min 1GB:1 || : # Ignore failures.
    sudo cpupower frequency-set --related --governor performance >/dev/null || : # Ignore failures.

    # Disable real-time throttling. Save its original value.
    if [[ -e ${sched_rt} ]]; then
        local tmp_orig_sched_rt=${orig_sched_rt}.${BASHPID}~
        [[ -e ${orig_sched_rt} ]] || cat ${sched_rt} > ${tmp_orig_sched_rt}
        echo -1 | {
            sudo tee ${sched_rt} >/dev/null
            [[ ! -e ${tmp_orig_sched_rt} ]] || mv -f ${tmp_orig_sched_rt} ${orig_sched_rt}
        }
    fi
}

function epilogue {
    sudo cpupower frequency-set --related --governor powersave >/dev/null || : # Ignore failures.

    # Restore original real-time throttling value.
    if [[ -f ${orig_sched_rt} ]]; then
        sudo tee ${sched_rt} >/dev/null <${orig_sched_rt} && rm ${orig_sched_rt}
    fi
}

function benchmark {
    trap epilogue EXIT
    prologue
    (set -e; exec "$@")
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    benchmark "$@"
fi
