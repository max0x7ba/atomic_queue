# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# Usage examples (assuming this directory is ~/src/atomic_queue):
#
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2))
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) all run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=gcc-14 BUILD=debug run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=clang-20 BUILD=debug run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=gcc-14 run_benchmarks_n
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=clang-20 run_benchmarks_n
#   taskset -c 0,1,2,3 time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=gcc-14 run_benchmarks_n N=2
#   taskset -c 0,2,4,6 time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=gcc-14 run_benchmarks_n N=2
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=gcc-14 ASM=1
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=clang-20 BUILD=debug TAGS
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) TOOLSET=clang BUILD=sanitize run_tests
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) BUILD=debug run_tests2
#
# Additional CPPFLAGS, CXXFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.  For example, also produce assembly outputs:
#
#   time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) CXXFLAGS="-save-temps=obj -fverbose-asm -masm=intel"
#

SHELL := /bin/bash
.SHELLFLAGS := --norc -o pipefail -e -c

export BUILD := release
export TOOLSET := gcc

export BUILD_ROOT := $(or ${BUILD_ROOT},build)
build_dir := ${BUILD_ROOT}/${BUILD}/${TOOLSET}

# Don't try loading localized messages for anything.
undefine LANG
undefine LANGUAGE
undefine LC_CTYPE
undefine LC_MESSAGES
undefine LC_ALL

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

export ASM := 0

uname_m := $(shell uname -m)
cxxflags.x86_64 := -fcf-protection=none -masm=intel

cxxflags.gcc.asm.1 := -save-temps=obj -fverbose-asm -fno-{stack-protector,stack-clash-protection}
cxxflags.gcc.debug := -Og -f{stack-protector-all,no-omit-frame-pointer} # -D_GLIBCXX_DEBUG
cxxflags.gcc.release := -O2 -mtune=native -fgcse-after-reload -momit-leaf-frame-pointer -falign-{functions,loops}=64 -DNDEBUG ${cxxflags.gcc.asm.${ASM}}
cxxflags.gcc.sanitize := ${cxxflags.gcc.debug} -fsanitize=thread
cxxflags.gcc.sanitize2 := ${cxxflags.gcc.debug} -fsanitize=undefined,address
cxxflags.gcc := -march=native -f{no-plt,no-math-errno,finite-math-only,message-length=0} -W{all,extra,error,no-{array-bounds,maybe-uninitialized,unused-variable,unused-function,unused-local-typedefs}} ${cxxflags.gcc.${BUILD}}
ldflags.gcc.sanitize := ${ldflags.gcc.debug} -fsanitize=thread
ldflags.gcc.sanitize2 := ${ldflags.gcc.debug} -fsanitize=undefined,address
# ldflags.gcc := -fuse-ld=${use-ld.gcc} -Wl,--compress-debug-sections=zstd,-O2,--gc-sections ${ldflags.gcc.${BUILD}}
ldflags.gcc := -fuse-ld=gold ${ldflags.gcc.${BUILD}}

# clang-14 for arm doesn't support -march=native.
has_native := $(if $(and $(findstring clang,${CXX}), $(findstring aarch64,$(shell uname -m)), $(shell ${CXX} -march=native -c -xc++ -o/dev/null /dev/null 2>&1)),,1)
cxxflags.clang.debug := -O0 -fstack-protector-all $(and ${has_native},-march=native)
cxxflags.clang.release := -O2 -falign-functions=64 -DNDEBUG $(and ${has_native},-march=native -mtune=native)
cxxflags.clang.sanitize := ${cxxflags.clang.debug} -fsanitize=thread
cxxflags.clang.sanitize2 := ${cxxflags.clang.debug} -fsanitize=undefined,address
cxxflags.clang := -stdlib=libstdc++ -f{no-plt,no-math-errno,finite-math-only,message-length=0} -W{all,extra,error,no-{unused-variable,unused-function,unused-local-typedefs}} ${cxxflags.clang.${BUILD}}
ldflags.clang.debug := -latomic # A work-around for clang bug.
ldflags.clang.sanitize := ${ldflags.clang.debug} -fsanitize=thread
ldflags.clang.sanitize2 := ${ldflags.clang.debug} -fsanitize=undefined,address
ldflags.clang := -stdlib=libstdc++ ${ldflags.clang.${BUILD}}

# Additional CPPFLAGS, CXXFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.
cxxflags := -std=c++14 -pthread -g $(call toolset_flags,cxxflags) ${cxxflags.${uname_m}} ${CXXFLAGS}
cppflags := -Iinclude ${CPPFLAGS}
ldflags := -pthread -g $(call toolset_flags,ldflags) ${LDFLAGS}
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

all :

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

all : ${exes}

define EXE_TARGET
${build_dir}/${1} : $(patsubst %.cc,${build_dir}/%.o,${${1}_src})
-include $(patsubst %.cc,${build_dir}/%.d,${${1}_src})
endef
$(foreach exe,${exes},$(eval $(call EXE_TARGET,${exe})))
${exes} : % : ${build_dir}/%
.PHONY : ${exes}
# for t in gcc gcc clang clang; do make -C ~/src/atomic_queue -rj$(($(nproc)/2)) BUILD=debug TOOLSET=$t; done

.SECONDEXPANSION:

${exes:%=${build_dir}/%} : ${build_dir}/% : ${relink} | $$(dir $$@)
	$(call strip2,${LINK.EXE})

${build_dir}/%.so : cxxflags += -fPIC
${build_dir}/%.so : ${relink} | $$(dir $$@)
	$(call strip2,${LINK.SO})

${build_dir}/%.a : ${relink} | $$(dir $$@)
	$(call strip2,${LINK.A})

${build_dir}/%.o : src/%.cc ${recompile} | $$(dir $$@)
	$(call strip2,${COMPILE.CXX})

src/%.cc :;
${build_dir}/%.d :;
Makefile :;

${build_dir}/.make ${build_dir}/.make/ perf/ results/ :
	mkdir -p $@

${BUILD_ROOT}/ ${build_dir} ${build_dir}/ : | ${build_dir}/.make/ ;


ver = "$(shell ${1} --version | ${head1})"
# Trigger recompilation when compiler environment change.
env.compile := $(call ver,${CXX}) ${cppflags} ${cxxflags} ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium} ${cxxflags.tbb} ${cxxflags.moodycamel} ${cxxflags.xenium}
# Trigger relink when linker environment change.
env.link := $(call ver,${LD}) ${ldflags} ${ldlibs} ${ldlibs.tbb} ${ldlibs.moodycamel} ${ldlibs.xenium}

define env_txt_rule
${build_dir}/.make/env.${1}.txt : $(shell cmp --quiet ${build_dir}/.make/env.${1}.txt <(printf "%s\n" ${env.${1}}) || echo update_env_txt) | ${build_dir}/.make/
	@printf "%s\n" ${env.${1}} >$$@
endef
$(eval $(call env_txt_rule,compile))
$(eval $(call env_txt_rule,link))
${recompile} ${relink} : ${build_dir}/.make/re% : ${build_dir}/.make/env.%.txt # Makefile
	@[[ ! -f $@ ]] || { u="$?"; echo "Re-$* is triggered by changes in $${u// /, }."; }
	touch $@
# cd ~/src/atomic_queue; make -r clean; set -x; make -rj8; make -rj8; make -rj8 CPPFLAGS=-DXYZ=1; make -rj8 CPPFLAGS=-DXYZ=1; make -rj8; make -rj8; make -rj8 LDLIBS=-lrt; make -rj8 LDLIBS=-lrt; make -rj8; make -rj8

include ${BUILD_ROOT}/chrt.mk
${BUILD_ROOT}/chrt.mk : | $$(dir $$@)
	echo "chrt_fifo := $$(chrt -f 50 printf 'chrt' || printf 'sudo chrt') -f 50" > $@

N := 1
TAG := results
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

run_tests.% : force
	${MAKE} --no-print-directory --output-sync TOOLSET=$* all run_tests

run_tests2 : run_tests.gcc-14 run_tests.clang-20

run_tests : ${build_dir}/tests
	@echo -n "$@ "; set -x; $< --log_level=unit_scope --report_level=short

run_% : ${build_dir}/% force
	@echo -n "$@ "; set -x; $<

compile_commands compile_commands.json TAGS :
	bear -- ${MAKE} --always-make --no-print-directory all

clean :
	rm -rf ${build_dir}

distclean :
	rm -rf ${BUILD_ROOT}

versions:
	${MAKE} --version | ${head1}
	${CXX} --version | ${head1}

env :
	uname --all
	env | sort --ignore-case

# Prerequisites of .PHONY are always interpreted as literal target names, never as patterns (even if they contain ‘%’ characters). To always rebuild a pattern rule consider using a "force target".
# If a rule has no prerequisites or recipe, and the target of the rule is a nonexistent file, then make imagines this target to have been updated whenever its rule is run. This implies that all targets depending on this one will always have their recipe run.
force :

.PHONY : update_env_txt env versions run_benchmarks clean all compile_commands compile_commands.json TAGS run_tests run_benchmarks_n run_benchmarks_perf run_tests2 distclean
.DELETE_ON_ERROR :
.SECONDARY :
.SUFFIXES :

# Local Variables:
# compile-command: "/bin/time make -C ~/src/atomic_queue -Rj$(($(nproc)/2)) BUILD=debug run_tests"
# End:
