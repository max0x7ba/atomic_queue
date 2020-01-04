#!/bin/bash

set +e # Ignore failures.

sudo cpupower frequency-set --related --governor powersave >/dev/null
~/scripts/cpu-fans.sh 3 >/dev/null
