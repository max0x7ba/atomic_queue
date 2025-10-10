# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# Usage examples (assuming this directory is ~/src/atomic_queue):
#
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2))
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) BUILD=debug run_tests
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) all run_tests
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) run_benchmarks
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) TOOLSET=clang-19 BUILD=debug run_tests
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) TOOLSET=clang BUILD=sanitize run_tests
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) TOOLSET=clang run_benchmarks
#
# Additional CPPFLAGS, CXXFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.  For example, also produce assembly outputs:
#
#   time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) CXXFLAGS="-save-temps=obj -fverbose-asm -masm=intel"
#

SHELL := /bin/bash

BUILD := release
TOOLSET := gcc

BUILD_ROOT := $(or ${BUILD_ROOT},build)
build_dir := ${BUILD_ROOT}/${BUILD}/${TOOLSET}

cxx.gcc := g++
cc.gcc := gcc
ld.gcc := g++
ar.gcc := gcc-ar

cxx.clang := clang++
cc.clang := clang
ld.clang := clang++
ar.clang := ar

toolset_family := $(or $(findstring gcc,${TOOLSET}),$(findstring clang,${TOOLSET}))
toolset_suffix := $(subst ${toolset_family},,${TOOLSET})
toolset_flags = $(or ${${1}.${TOOLSET}},${${1}.${toolset_family}},$(error ${1}.${TOOLSET} is undefined))
toolset_exe = $(or ${${1}.${TOOLSET}},$(and ${${1}.${toolset_family}},${${1}.${toolset_family}}${toolset_suffix}),${2},$(error ${1}.${TOOLSET} is undefined))

CXX := $(call toolset_exe,cxx)
CC := $(call toolset_exe,cc)
LD := $(call toolset_exe,ld)
AR := $(call toolset_exe,ar,ar)

# uname_m := $(shell uname -m)

cxxflags.gcc.debug := -Og -march=native -fstack-protector-all -fno-omit-frame-pointer # -D_GLIBCXX_DEBUG
cxxflags.gcc.release := -O3 -march=native -mtune=native -falign-{functions,loops}=64 -DNDEBUG
cxxflags.gcc.sanitize := ${cxxflags.gcc.release} -fsanitize=thread
cxxflags.gcc := -g -f{no-plt,no-math-errno,finite-math-only,message-length=0} -W{all,extra,error,no-{array-bounds,maybe-uninitialized,unused-variable,unused-function,unused-local-typedefs}} ${cxxflags.gcc.${BUILD}}
ldflags.gcc.sanitize := ${ldflags.gcc.release} -fsanitize=thread
ldflags.gcc := -g ${ldflags.gcc.${BUILD}}

# clang-14 for arm doesn't support -march=native.
has_native := $(if $(and $(findstring clang,${CXX}), $(findstring aarch64,$(shell uname -m)), $(shell ${CXX} -march=native -c -xc++ -o/dev/null /dev/null 2>&1)),,1)
cxxflags.clang.debug := -O0 -fstack-protector-all $(and ${has_native},-march=native)
cxxflags.clang.release := -O3 -falign-functions=64 -DNDEBUG $(and ${has_native},-march=native -mtune=native)
cxxflags.clang.sanitize := ${cxxflags.clang.release} -fsanitize=thread
cxxflags.clang := -g -stdlib=libstdc++ -f{no-plt,no-math-errno,finite-math-only,message-length=0} -W{all,extra,error,no-{unused-variable,unused-function,unused-local-typedefs}} ${cxxflags.clang.${BUILD}}
ldflags.clang.sanitize := ${ldflags.clang.release} -fsanitize=thread
ldflags.clang.debug := -latomic # A work-around for clang bug.
ldflags.clang := -g -stdlib=libstdc++ ${ldflags.clang.${BUILD}}

# Additional CPPFLAGS, CXXFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.
cxxflags := -std=c++14 -pthread $(call toolset_flags,cxxflags) ${CXXFLAGS}
cppflags := -Iinclude ${CPPFLAGS}
ldflags := -fuse-ld=gold -pthread $(call toolset_flags,ldflags) ${LDFLAGS}
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

all : ${exes:%=${build_dir}/%}

define EXE_TARGET
${build_dir}/${1} : $(patsubst %.cc,${build_dir}/%.o,${${1}_src})
-include $(patsubst %.cc,${build_dir}/%.d,${${1}_src})
${1} : ${build_dir}/${1}
.PHONY : ${1}
endef
$(foreach exe,${exes},$(eval $(call EXE_TARGET,${exe})))

${exes:%=${build_dir}/%} : ${build_dir}/% : ${relink} | ${build_dir}
	$(call strip2,${LINK.EXE})

# ${exes} : % : ${build_dir}/%
# 	ln --relative -fs $@

${build_dir}/%.so : cxxflags += -fPIC
${build_dir}/%.so : ${relink} | ${build_dir}
	$(call strip2,${LINK.SO})

${build_dir}/%.a : ${relink} | ${build_dir}
	$(call strip2,${LINK.A})

${build_dir}/%.o : src/%.cc ${recompile} | ${build_dir}
	$(call strip2,${COMPILE.CXX})

${build_dir}/%.d : ;

${build_dir}/.make : | ${build_dir}
${build_dir} ${build_dir}/.make:
	mkdir -p $@

ver = "$(shell ${1} --version | head -n1)"
# Trigger recompilation when compiler environment change.
env.compile := $(call ver,${CXX}) ${cppflags} ${cxxflags} ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium} ${cxxflags.tbb} ${cxxflags.moodycamel} ${cxxflags.xenium}
# Trigger relink when linker environment change.
env.link := $(call ver,${LD}) ${ldflags} ${ldlibs} ${ldlibs.tbb} ${ldlibs.moodycamel} ${ldlibs.xenium}

define env_txt_rule
${build_dir}/.make/env.${1}.txt : $(shell cmp --quiet ${build_dir}/.make/env.${1}.txt <(printf "%s\n" ${env.${1}}) || echo update_env_txt) Makefile | ${build_dir}/.make
	@printf "%s\n" ${env.${1}} >$$@
endef
$(eval $(call env_txt_rule,compile))
$(eval $(call env_txt_rule,link))
${recompile} ${relink} : ${build_dir}/.make/re% : ${build_dir}/.make/env.%.txt Makefile
	@[[ ! -f $@ ]] || { u="$?"; echo "Re-$* is triggered by changes in $${u// /, }."; }
	touch $@
# cd ~/src/atomic_queue; make -r clean; set -x; make -rj8; make -rj8; make -rj8 CPPFLAGS=-DXYZ=1; make -rj8 CPPFLAGS=-DXYZ=1; make -rj8; make -rj8; make -rj8 LDLIBS=-lrt; make -rj8 LDLIBS=-lrt; make -rj8; make -rj8

run_benchmarks : ${build_dir}/benchmarks
	@echo -n "$@ "; set -x; scripts/benchmark.sh sudo chrt -f 50 $<

run_tests : ${build_dir}/tests
	@echo -n "$@ "; set -x; $< --log_level=warning

run_% : ${build_dir}/%
	@echo -n "$@ "; set -x; $<

rtags :
	${MAKE} --always-make --just-print all | { rtags-rc -c -; true; }

clean :
	rm -rf ${exes} ${build_dir}

versions:
	${MAKE} --version | awk 'FNR<2'
	${CXX} --version | head -n1

env :
	uname --all
	env | sort --ignore-case

.PHONY : update_env_txt env versions rtags run_benchmarks clean all run_%
.DELETE_ON_ERROR:
.SECONDARY:
.SUFFIXES:

# Local Variables:
# compile-command: "/bin/time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) BUILD=debug run_tests"
# End:
