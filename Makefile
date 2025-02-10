# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# Usage examples (assuming this directory is ~/src/atomic_queue):
#
#   time make -rC ~/src/atomic_queue -j8
#   time make -rC ~/src/atomic_queue -j8 run_benchmarks
#   time make -rC ~/src/atomic_queue -j8 TOOLSET=clang run_benchmarks
#   time make -rC ~/src/atomic_queue -j8 BUILD=debug run_tests
#   time make -rC ~/src/atomic_queue -j8 BUILD=sanitize TOOLSET=clang run_tests
#
# Additional CPPFLAGS, CXXFLAGS, CFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.  For example, also produce assembly outputs:
#
#   time make -rC ~/src/atomic_queue -j8 CXXFLAGS="-save-temps=obj -fverbose-asm -masm=intel"
#

SHELL := /bin/bash

BUILD := release
TOOLSET := gcc

build_dir := ${CURDIR}/build/${BUILD}/${TOOLSET}

cxx.gcc := g++
cc.gcc := gcc
ld.gcc := g++
ar.gcc := gcc-ar

cxx.clang := clang++
cc.clang := clang
ld.clang := clang++
ar.clang := ar

CXX := ${cxx.${TOOLSET}}
CC := ${cc.${TOOLSET}}
LD := ${ld.${TOOLSET}}
AR := ${ar.${TOOLSET}}

cxxflags.gcc.debug := -Og -fstack-protector-all -fno-omit-frame-pointer # -D_GLIBCXX_DEBUG
cxxflags.gcc.release := -O3 -mtune=native -ffast-math -falign-{functions,loops}=64 -DNDEBUG
cxxflags.gcc.sanitize := ${cxxflags.gcc.release} -fsanitize=thread
cxxflags.gcc := -std=gnu++14 -pthread -march=native -W{all,extra,error,no-{array-bounds,maybe-uninitialized,unused-variable,unused-function,unused-local-typedefs}} -fmessage-length=0 ${cxxflags.gcc.${BUILD}}
ldflags.gcc.sanitize := ${ldflags.gcc.release} -fsanitize=thread
ldflags.gcc := ${ldflags.gcc.${BUILD}}

cflags.gcc := -pthread -march=native -W{all,extra} -fmessage-length=0 ${cxxflags.gcc.${BUILD}}

cxxflags.clang.debug := -O0 -fstack-protector-all
cxxflags.clang.release := -O3 -mtune=native -ffast-math -falign-functions=64 -DNDEBUG
cxxflags.clang.sanitize := ${cxxflags.clang.release} -fsanitize=thread
cxxflags.clang := -std=gnu++14 -pthread -march=native -stdlib=libstdc++ -W{all,extra,error,no-{unused-variable,unused-function,unused-local-typedefs}} -fmessage-length=0 ${cxxflags.clang.${BUILD}}
ldflags.clang.sanitize := ${ldflags.clang.release} -fsanitize=thread
ldflags.clang.debug := -latomic
ldflags.clang := -stdlib=libstdc++ ${ldflags.clang.${BUILD}}

# Additional CPPFLAGS, CXXFLAGS, CFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.
cxxflags := ${cxxflags.${TOOLSET}} -g ${CXXFLAGS}
cflags := ${cflags.${TOOLSET}} -g ${CFLAGS}
cppflags := -Iinclude ${CPPFLAGS}
ldflags := -fuse-ld=gold -pthread -g ${ldflags.${TOOLSET}} ${LDFLAGS}
ldlibs := -lrt ${LDLIBS}

cppflags.tbb :=
ldlibs.tbb := {-L,'-Wl,-rpath='}/usr/local/lib -ltbb

cppflags.moodycamel := -I$(abspath ..)
ldlibs.moodycamel :=

cppflags.xenium := -I${abspath ../xenium}
cxxflags.xenium := -std=gnu++17
ldlibs.xenium :=

recompile := ${build_dir}/.make/recompile
relink := ${build_dir}/.make/relink

COMPILE.CXX = ${CXX} -o $@ -c ${cppflags} ${cxxflags} -MD -MP $(abspath $<)
COMPILE.S = ${CXX} -o- -S -fverbose-asm -masm=intel ${cppflags} ${cxxflags} $(abspath $<) | c++filt | egrep -v '^[[:space:]]*\.(loc|cfi|L[A-Z])' > $@
PREPROCESS.CXX = ${CXX} -o $@ -E ${cppflags} ${cxxflags} $(abspath $<)
COMPILE.C = ${CC} -o $@ -c ${cppflags} ${cflags} -MD -MP $(abspath $<)
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

exes := benchmarks tests example

all : ${exes}

${exes} : % : ${build_dir}/%
	ln -sf ${<:${CURDIR}/%=%}

benchmarks_src := benchmarks.cc cpu_base_frequency.cc huge_pages.cc
${build_dir}/benchmarks : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/benchmarks : cxxflags += ${cxxflags.tbb} ${cxxflags.moodycamel} ${cxxflags.xenium}
${build_dir}/benchmarks : ldlibs += ${ldlibs.tbb} ${ldlibs.moodycamel} ${ldlibs.xenium} -ldl
${build_dir}/benchmarks : ${benchmarks_src:%.cc=${build_dir}/%.o} ${relink} | ${build_dir}
	$(call strip2,${LINK.EXE})
-include ${benchmarks_src:%.cc=${build_dir}/%.d}

tests_src := tests.cc
${build_dir}/tests : cppflags += -DBOOST_TEST_DYN_LINK=1
${build_dir}/tests : ldlibs += -lboost_unit_test_framework
${build_dir}/tests : ${tests_src:%.cc=${build_dir}/%.o} ${relink} | ${build_dir}
	$(call strip2,${LINK.EXE})
-include ${tests_src:%.cc=${build_dir}/%.d}

example_src := example.cc
${build_dir}/example : ${example_src:%.cc=${build_dir}/%.o} ${relink} | ${build_dir}
	$(call strip2,${LINK.EXE})
-include ${example_src:%.cc=${build_dir}/%.d}

${build_dir}/%.so : cxxflags += -fPIC
${build_dir}/%.so : ${relink} | ${build_dir}
	$(call strip2,${LINK.SO})

${build_dir}/%.a : ${relink} | ${build_dir}
	$(call strip2,${LINK.A})

${build_dir}/%.o : src/%.cc ${recompile} | ${build_dir}
	$(call strip2,${COMPILE.CXX})

${build_dir}/%.o : src/%.c ${recompile} | ${build_dir}
	$(call strip2,${COMPILE.C})

${build_dir}/%.S : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/%.S : src/%.cc ${recompile} | ${build_dir}
	$(call strip2,${COMPILE.S})

${build_dir}/%.I : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/%.I : src/%.cc ${recompile} | ${build_dir}
	$(call strip2,${PREPROCESS.CXX})

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
	@echo "---- running $< ----"
	scripts/benchmark-prologue.sh
	-sudo chrt -f 50 $<
	scripts/benchmark-epilogue.sh

run_tests : ${build_dir}/tests
	@echo "---- running $< ----"
	$<

run_% : ${build_dir}/%
	@echo "---- running $< ----"
	$<

rtags :
	${MAKE} --always-make --just-print all | { rtags-rc -c -; true; }

clean :
	rm -rf ${build_dir} ${exes}

versions:
	${MAKE} --version | awk 'FNR<2'
	${CXX} --version | head -n1

env :
	env | sort --ignore-case

.PHONY : update_env_txt env versions rtags run_benchmarks clean all run_%

# Local Variables:
# compile-command: "/bin/time make -rC ~/src/atomic_queue -j$(($(nproc)/2)) BUILD=debug run_tests"
# End:
