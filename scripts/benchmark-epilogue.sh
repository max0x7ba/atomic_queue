#!/bin/bash

set +e # Ignore failures.

sudo cpupower frequency-set --related --governor powersave >/dev/null

[[ -f .sched_rt_runtime_us.txt ]] && sudo tee /proc/sys/kernel/sched_rt_runtime_us >/dev/null <.sched_rt_runtime_us.txt
