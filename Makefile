# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# Usage examples (assuming this directory is ~/src/atomic_queue):
#
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2))
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) all run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) T=1 TOOLSET=gcc-14 BUILD=debug run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) T=1 TOOLSET=clang-20 BUILD=debug run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) T=1 TOOLSET=gcc-14 run_benchmarks_n
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) T=1 TOOLSET=clang-20 run_benchmarks_n
#   taskset -c 0,1,2,3 time make -C ~/src/atomic_queue -Rj4 T=1 TOOLSET=gcc-14 run_benchmarks_n N=2
#   taskset -c 0,2,4,6 time make -C ~/src/atomic_queue -Rj4 T=1 TOOLSET=gcc-14 run_benchmarks_n N=2
#   taskset -c $(seq -s, 0 2 15) time make -C ~/src/atomic_queue -Rj8 T=1 TOOLSET=gcc-14 run_benchmarks_n N=33 TAG=cross-core
#   taskset -c 4-7 time make -C ~/src/atomic_queue -Rj4 TOOLSET=gcc-14 run_benchmarks_n N=2 TAG=statev1
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=gcc-14 ASM=1
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=clang-20 BUILD=debug TAGS
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=clang BUILD=sanitize run_tests
#
# Build and run with multiple toolsets in parallel:
#
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) T=1 TOOLSET=gcc,gcc-14,clang,clang-20 BUILD=debug all run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) T=1 TOOLSET=gcc,gcc-14,clang,clang-20 CPPFLAGS="-DATOMIC_QUEUE_REMAP=RemapAnd" all run_tests
#   printf "%s\n" distclean "all run_tests" | xargs -Iargs time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) T=1 TOOLSET=gcc,gcc-14,clang,clang-20 BUILD=debug args
#
# Additional CPPFLAGS, CXXFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.  For example, also produce assembly outputs:
#
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) CPPFLAGS="-DATOMIC_QUEUE_REMAP=RemapAnd"
#

################################################################################################################################
# The defaults.

# Easy BUILD_ROOT selection by setting T variable.
T := 0
build_root.0 := build   # T=0 builds into build sub-directory.
build_root.1 := /tmp/b1 # T=1 builds into /tmp/b1.
build_root.2 := /tmp/b2 # T=2 builds into /tmp/b2.

# Tedious BUILD_ROOT selection by setting BUILD_ROOT variable.
BUILD_ROOT := $(strip $(or ${build_root.${T}},$(error build_root.${T} is undefined.)))

BUILD := release
TOOLSET := gcc
ASM := 0

################################################################################################################################
# Name of the benchmarks experiment and the number of iterations.
TAG := results
N := 1

################################################################################################################################
# Enables GNU Make to enable the jobserver protocol for the toolset programs to request allocating more threads.
JOBSERVER := 1
J.0 :=
J.1 := +
J := ${J.${JOBSERVER}}

################################################################################################################################

override undefine has_system_config

ifneq (,$(findstring clean,${MAKECMDGOALS}))
override cleaning := 1
override undefine not_cleaning
else
override undefine cleaning
override not_cleaning := 1
include ${BUILD_ROOT}/system_config.mk
endif

################################################################################################################################

colors := $(shell echo "\033[97m \033[0m")
c7 := $(word 1,${colors})
c0 := $(word 2,${colors})

log_src := $(firstword ${MAKEFILE_LIST})
log = $(info ${log_src}: ${c7}${1}${c0})
log_kv = $(let head tail,${1},$(call log,$(head)=$(value $(head)))$(and $(tail),$(call log_kv,$(tail))))

n_cpus := $(or $(subst -j,,$(filter -j%,${MAKEFLAGS})),1)

################################################################################################################################
# Boiler-plate begin.

SHELL := /bin/bash
.SHELLFLAGS := --norc -o pipefail -e -c

# Don't try loading localized messages for anything.
undefine LANG
undefine LANGUAGE
undefine LC_CTYPE
undefine LC_MESSAGES
undefine LC_ALL

comma := ,
empty :=
space := ${empty} ${empty}
space-to-comma = $(subst ${space},${comma},${1})
comma-to-space = $(subst ${comma},${space},${1})

# TOOLSET can be a list of comma-separated values.
toolsets := $(call comma-to-space,${TOOLSET})
mutli_toolset := $(intcmp $(words ${toolsets}),2,,1)

all :
.SECONDEXPANSION :

################################################################################################################################
ifeq (,${mutli_toolset}) # Build with a single toolset.
build_dir := ${BUILD_ROOT}/${BUILD}/${TOOLSET}

cxx.gcc := g++
cc.gcc := gcc
ld.gcc := g++
ar.gcc := gcc-ar

cxx.clang := clang++
cc.clang := clang
ld.clang := clang++
ar.clang := ar

head1 := /bin/awk 'FNR<2'
lb := /bin/stdbuf -oL

toolset_family := $(or $(findstring gcc,${TOOLSET}),$(findstring clang,${TOOLSET}))
toolset_suffix := $(subst ${toolset_family},,${TOOLSET})
toolset_flags = $(or ${${1}.${TOOLSET}},${${1}.${toolset_family}},$(error ${1}.${TOOLSET} is undefined))
toolset_exe = $(or ${${1}.${TOOLSET}},$(and ${${1}.${toolset_family}},${${1}.${toolset_family}}${toolset_suffix}),${2},$(error ${1}.${TOOLSET} is undefined))

CXX := $(call toolset_exe,cxx)
CC := $(call toolset_exe,cc)
LD := $(call toolset_exe,ld)
AR := $(call toolset_exe,ar,ar)

cxxflags.x86_64 := -fcf-protection=none -masm=intel

cxxflags.gcc.asm.1 := -save-temps=obj -fverbose-asm -fno-{stack-protector,stack-clash-protection}
cxxflags.gcc.debug := -Og -f{stack-protector-all,no-omit-frame-pointer} # -D_GLIBCXX_DEBUG
cxxflags.gcc.release := -O3 -mtune=native -f{no-stack-protector,align-{functions,loops}=64} -DNDEBUG ${cxxflags.gcc.asm.${ASM}}
cxxflags.gcc.sanitize := ${cxxflags.gcc.debug} -fsanitize=thread
cxxflags.gcc.sanitize2 := ${cxxflags.gcc.debug} -fsanitize=undefined,address
cxxflags.gcc := -march=native -f{no-plt,no-math-errno,finite-math-only,message-length=0} -W{all,extra,error,no-{array-bounds,maybe-uninitialized,unused-variable,unused-function,unused-local-typedefs}} ${cxxflags.gcc.${BUILD}}
ldflags.gcc.sanitize := ${ldflags.gcc.debug} -fsanitize=thread
ldflags.gcc.sanitize2 := ${ldflags.gcc.debug} -fsanitize=undefined,address
# ldflags.gcc := -fuse-ld=${use-ld.gcc} -Wl,--compress-debug-sections=zstd,-O2,--gc-sections ${ldflags.gcc.${BUILD}}
ldflags.gcc := -g ${use_ld} ${ldflags.gcc.${BUILD}}

# clang-14 for arm doesn't support -march=native.
has_native := $(if $(and $(findstring clang,${CXX}), $(findstring aarch64,$(shell uname -m)), $(shell ${CXX} -march=native -c -xc++ -o/dev/null /dev/null 2>&1)),,1)
cxxflags.clang.debug := -O0 -fstack-protector-all $(and ${has_native},-march=native)
cxxflags.clang.release := -O3 -f{no-stack-protector,align-functions=64} -DNDEBUG $(and ${has_native},-march=native -mtune=native)
cxxflags.clang.sanitize := ${cxxflags.clang.debug} -fsanitize=thread
cxxflags.clang.sanitize2 := ${cxxflags.clang.debug} -fsanitize=undefined,address
cxxflags.clang := -stdlib=libstdc++ -f{no-plt,no-math-errno,finite-math-only,message-length=0} -W{all,extra,error,no-{unused-variable,unused-function,unused-local-typedefs}} ${cxxflags.clang.${BUILD}}
ldflags.clang.debug := -latomic # A work-around for clang bug.
ldflags.clang.sanitize := ${ldflags.clang.debug} -fsanitize=thread
ldflags.clang.sanitize2 := ${ldflags.clang.debug} -fsanitize=undefined,address
ldflags.clang := -g ${use_ld} -stdlib=libstdc++ ${ldflags.clang.${BUILD}}

# Additional CPPFLAGS, CXXFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.
cxxflags := -std=c++14 -pthread -g $(call toolset_flags,cxxflags) ${cxxflags.${uname_m}} ${CXXFLAGS}
cppflags := -Iinclude ${CPPFLAGS}
ldflags := -pthread $(call toolset_flags,ldflags) -Wl,-z,norelro,-z,now,-z,max-page-size=0x200000,-z,common-page-size=0x200000,-z,separate-code,--build-id=none ${LDFLAGS}
ldlibs := -lrt ${LDLIBS}

cppflags.tbb :=
ldlibs.tbb := {-L,'-Wl,-rpath='}/usr/local/lib -ltbb

cppflags.moodycamel := -I..
ldlibs.moodycamel :=

cppflags.xenium := -I../xenium
cxxflags.xenium := -std=c++17
ldlibs.xenium :=

recompile := ${build_dir}/.make/recompile
relink := ${build_dir}/.make/relink

COMPILE.CXX = ${CXX} -o $@ -c ${cppflags} ${cxxflags} -MD -MP $<
LINK.EXE = ${LD} -o $@ $(ldflags) $(filter-out ${relink},$^) $(ldlibs)
LINK.SO = ${LD} -o $@ -shared $(ldflags) $(filter-out ${relink},$^) $(ldlibs)
LINK.A = ${AR} rscT $@ $(filter-out ${relink},$^)

ifneq (,$(findstring n,$(firstword -${MAKEFLAGS})))
# Perform bash parameter expansion when --just-print for rtags.
strip2 = $(shell printf '%q ' ${1})
else
# Unduplicate whitespace.
strip2 = $(strip ${1})
endif

# Boiler-plate end.
################################################################################################################################
# Build targets definitions begin.

exes := example tests benchmarks

example_src := example.cc

tests_src := tests.cc
${build_dir}/tests : cppflags += -DBOOST_TEST_DYN_LINK=1
${build_dir}/tests : ldlibs += -lboost_unit_test_framework

benchmarks_src := benchmarks.cc cpu_base_frequency.cc huge_pages.cc
${build_dir}/benchmarks : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/benchmarks : cxxflags += ${cxxflags.tbb} ${cxxflags.moodycamel} ${cxxflags.xenium}
${build_dir}/benchmarks : ldlibs += ${ldlibs.tbb} ${ldlibs.moodycamel} ${ldlibs.xenium} -ldl

# Build targets definitions end.
################################################################################################################################
# Boiler-plate begin.

all : ${exes}

auto_generated_header_d :=

define EXE_TARGET
${build_dir}/${1} : $(patsubst %.cc,${build_dir}/%.o,${${1}_src})
auto_generated_header_d += $(patsubst %.cc,${build_dir}/%.d,${${1}_src})
endef
$(foreach exe,${exes},$(eval $(call EXE_TARGET,${exe})))
${exes} : % : ${build_dir}/%
.PHONY : ${exes}
# for t in gcc gcc clang clang; do make -C ~/src/atomic_queue -rj$(($(nproc)/2)) BUILD=debug TOOLSET=$t; done


${exes:%=${build_dir}/%} : ${build_dir}/% : ${relink} | $$(dir $$@)
	${J}$(call strip2,${LINK.EXE})

${build_dir}/%.so : cxxflags += -fPIC
${build_dir}/%.so : ${relink} | $$(dir $$@)
	${J}$(call strip2,${LINK.SO})

${build_dir}/%.a : ${relink} | $$(dir $$@)
	${J}$(call strip2,${LINK.A})

${build_dir}/%.o : src/%.cc ${recompile} | $$(dir $$@)
	${J}$(call strip2,${COMPILE.CXX})

################################################################################################################################
# Compiler and linker options tracking.

ver = "$(shell ${1} --version | ${head1})"

# Trigger recompilation when compiler environment change.
${recompile} : private print := printf "%s\n" $(call strip2,$(call ver,${CXX}) ${cppflags} ${cxxflags} ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium} ${cxxflags.tbb} ${cxxflags.moodycamel} ${cxxflags.xenium} ${ASM})

# Trigger relink when linker environment change.
${relink} : private print := printf "%s\n" $(call strip2,$(call ver,${LD}) ${ldflags} ${ldlibs} ${ldlibs.tbb} ${ldlibs.moodycamel} ${ldlibs.xenium})

${recompile} ${relink} : ${build_dir}/.make/% : $$(shell /bin/cmp --quiet $$@ <($${print}) || echo update_env_txt) | $$(dir $$@)
	${print} > $@

# cd ~/src/atomic_queue; make -r clean; set -x; make -rj8; make -rj8; make -rj8 CPPFLAGS=-DXYZ=1; make -rj8 CPPFLAGS=-DXYZ=1; make -rj8; make -rj8; make -rj8 LDLIBS=-lrt; make -rj8 LDLIBS=-lrt; make -rj8; make -rj8

################################################################################################################################

new_filename = $(shell date "+${TAG}.%Y%m%dT%H%M%S.${TOOLSET}.$$(nproc)")

results/%.txt : ${build_dir}/benchmarks | $$(dir $$@)
	{ for((i=1;i<=${N};++i)); do printf "\n%(%F %T)T [$$i/${N}] "; ${chrt_fifo} ${lb} /bin/time -v $<; echo; done; } |& tee -i $@

perf/%.txt : ${build_dir}/benchmarks | $$(dir $$@)
	{ printf "\n%(%F %T)T "; ${chrt_fifo} ${lb} perf stat -dd $< ; echo; } |& tee -i $@

.PRECIOUS : perf/%.txt results/%.txt # Don't delete these on error.

run_benchmarks_n : results/$${new_filename}.${N}.txt
	@printf "%(%F %T)T $@ saved \e[32m$(abspath $<)\e[0m\n\n"

run_benchmarks_perf : perf/$${new_filename}.txt
	@printf "%(%F %T)T $@ saved \e[32m$(abspath $<)\e[0m\n\n"

run_benchmarks : ${build_dir}/benchmarks
	@echo -n "$@ "; set -x; scripts/benchmark.sh ${chrt_fifo} $<

run_tests : ${build_dir}/tests
	@echo -n "$@ "; set -x; $< --log_level=unit_scope --report_level=short

run_% : ${build_dir}/% force
	@echo -n "$@ "; set -x; $<

compile_commands compile_commands.json TAGS :
	bear -- ${MAKE} -R --always-make --no-print-directory all

clean :
	rm -rf ${build_dir}

versions:
	${MAKE} --version | ${head1}
	${CXX} --version | ${head1}

${build_dir}/.make/ perf/ results/ :
	mkdir -p $@

${build_dir}/ : | ${build_dir}/.make/ ;

env2 : env

.PHONY : update_env_txt env versions run_benchmarks clean all compile_commands compile_commands.json TAGS run_tests run_benchmarks_n run_benchmarks_perf env2

ifeq (,$(findstring clean,${MAKECMDGOALS})) # Not cleanining.
-include $(sort ${auto_generated_header_d}) # Remove duplicates and include.
endif # Not cleanining.

endif # Build with a single toolset.

################################################################################################################################

ifeq (,$(findstring clean,${MAKECMDGOALS})) # Not cleanining.

${BUILD_ROOT}/system_config.mk : scripts/util.sh | $$(dir $$@)
	source $< && create_system_config_mk > $@

endif # Not cleanining.

################################################################################################################################

${BUILD_ROOT}/ :
	mkdir -p $@

.PHONY :  distclean
distclean :
	rm -rf ${BUILD_ROOT}

env :
	uname --all
	env | sort --ignore-case

# Prerequisites of .PHONY are always interpreted as literal target names, never as patterns (even if they contain ‘%’ characters). To always rebuild a pattern rule consider using a "force target".
# If a rule has no prerequisites or recipe, and the target of the rule is a nonexistent file, then make imagines this target to have been updated whenever its rule is run. This implies that all targets depending on this one will always have their recipe run.
force :

# Tell make to never consider updating any of these:
Makefile :;
scripts/% :;
include/% :;
src/% :;
%.d :;

.SUFFIXES : # Disable the built-in GNU Make rules. make -R is still more efficient.
.DELETE_ON_ERROR :
.SECONDARY :

################################################################################################################################
ifdef has_system_config

targets := $(or ${MAKECMDGOALS},${.DEFAULT_GOAL})

timestamp_usec = $(shell printf "%.0f" $${EPOCHREALTIME}e+6)
t0 := ${timestamp_usec}

all :
	@printf "${log_src}: ${c7}%s made targets '%s' in %'.3f seconds.${c0}\n" "${TOOLSET}" "${targets}" "$$((${timestamp_usec} - ${t0}))"e-6

################################################################################################################################
ifneq (,${mutli_toolset}) # Parallelize building with multiple toolsets.

$(call log,Build targets "${targets}" with toolsets "${toolsets}" in parallel using up to ${n_cpus} CPUs.)
$(intcmp ${MAKELEVEL},1,$(call log_kv,BUILD_ROOT))

with-toolset-% :
	${MAKE} -R --no-print-directory --output-sync TOOLSET=$* with_toolset_sub_make=1 ${MAKECMDGOALS}

# The last-resort rule. Must be the last in the Makefile.
% : ${toolsets:%=with-toolset-%}
	@printf "${log_src}: ${c7}%s made targets '%s' in %'.3f seconds.${c0}\n" "${TOOLSET}" "${targets}" "$$((${timestamp_usec} - ${t0}))"e-6

################################################################################################################################
else # Parallelize building with multiple toolsets.

ifndef with_toolset_sub_make
$(call log,Build targets "${targets}" with ${TOOLSET} using up to ${n_cpus} CPUs.)
$(intcmp ${MAKELEVEL},1,$(call log_kv,BUILD_ROOT))
endif

endif # Parallelize building with multiple toolsets.
################################################################################################################################

endif # has_system_config
################################################################################################################################

# Local Variables:
# compile-command: "/bin/time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) BUILD=debug run_tests"
# End:
