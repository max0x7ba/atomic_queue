# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# Usage examples (assuming this directory is ~/src/atomic_queue):
# time make -rC ~/src/atomic_queue -j8 run_benchmarks
# time make -rC ~/src/atomic_queue -j8 TOOLSET=clang run_benchmarks
# time make -rC ~/src/atomic_queue -j8 BUILD=debug run_tests

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
cxxflags.gcc := -pthread -march=native -std=gnu++14 -W{all,extra,error,no-{maybe-uninitialized,unused-variable,unused-function,unused-local-typedefs}} -g -fmessage-length=0 ${cxxflags.gcc.${BUILD}}

cflags.gcc := -pthread -march=native -W{all,extra} -g -fmessage-length=0 ${cxxflags.gcc.${BUILD}}

cxxflags.clang.debug := -O0 -fstack-protector-all
cxxflags.clang.release := -O3 -mtune=native -ffast-math -falign-functions=64 -DNDEBUG
cxxflags.clang := -stdlib=libstdc++ -pthread -march=native -std=gnu++14 -W{all,extra,error,no-{unused-variable,unused-function,unused-local-typedefs}} -g -fmessage-length=0 ${cxxflags.clang.${BUILD}}
ldflags.clang := -stdlib=libstdc++ ${ldflags.clang.${BUILD}}

# Additional CPPFLAGS, CXXFLAGS, CFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.
# However, a clean build is required when changing the flags in the command line or in environment variables, this makefile doesn't detect such changes.
cxxflags := ${cxxflags.${TOOLSET}} ${CXXFLAGS}
cflags := ${cflags.${TOOLSET}} ${CFLAGS}
cppflags := ${CPPFLAGS} -Iinclude
ldflags := -fuse-ld=gold -pthread -g ${ldflags.${TOOLSET}} ${LDFLAGS}
ldlibs := -lrt ${LDLIBS}

cppflags.tbb :=
ldlibs.tbb := {-L,'-Wl,-rpath='}/usr/local/lib -ltbb

cppflags.moodycamel := -I$(abspath ..)
ldlibs.moodycamel :=

cppflags.xenium := -I${abspath ../xenium}
ldlibs.xenium :=

COMPILE.CXX = ${CXX} -o $@ -c ${cppflags} ${cxxflags} -MD -MP $(abspath $<)
COMPILE.S = ${CXX} -o- -S -masm=intel ${cppflags} ${cxxflags} $(abspath $<) | c++filt | egrep -v '^[[:space:]]*\.(loc|cfi|L[A-Z])' > $@
PREPROCESS.CXX = ${CXX} -o $@ -E ${cppflags} ${cxxflags} $(abspath $<)
COMPILE.C = ${CC} -o $@ -c ${cppflags} ${cflags} -MD -MP $(abspath $<)
LINK.EXE = ${LD} -o $@ $(ldflags) $(filter-out Makefile,$^) $(ldlibs)
LINK.SO = ${LD} -o $@ -shared $(ldflags) $(filter-out Makefile,$^) $(ldlibs)
LINK.A = ${AR} rscT $@ $(filter-out Makefile,$^)

exes := benchmarks tests example

all : ${exes}

${exes} : % : ${build_dir}/%
	ln -sf ${<:${CURDIR}/%=%}

benchmarks_src := benchmarks.cc cpu_base_frequency.cc huge_pages.cc
${build_dir}/benchmarks : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/benchmarks : ldlibs += ${ldlibs.tbb} ${ldlibs.moodycamel} ${ldlibs.xenium} -ldl
${build_dir}/benchmarks : ${benchmarks_src:%.cc=${build_dir}/%.o} Makefile | ${build_dir}
	$(strip ${LINK.EXE})
-include ${benchmarks_src:%.cc=${build_dir}/%.d}

tests_src := tests.cc
${build_dir}/tests : cppflags += ${boost_unit_test_framework_inc} -DBOOST_TEST_DYN_LINK=1
${build_dir}/tests : ldlibs += -lboost_unit_test_framework
${build_dir}/tests : ${tests_src:%.cc=${build_dir}/%.o} Makefile | ${build_dir}
	$(strip ${LINK.EXE})
-include ${tests_src:%.cc=${build_dir}/%.d}

example_src := example.cc
${build_dir}/example : ${example_src:%.cc=${build_dir}/%.o} Makefile | ${build_dir}
	$(strip ${LINK.EXE})
-include ${example_src:%.cc=${build_dir}/%.d}

${build_dir}/%.so : cxxflags += -fPIC
${build_dir}/%.so : Makefile | ${build_dir}
	$(strip ${LINK.SO})

${build_dir}/%.a : Makefile | ${build_dir}
	$(strip ${LINK.A})

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

${build_dir}/%.o : src/%.cc Makefile | ${build_dir}
	$(strip ${COMPILE.CXX})

${build_dir}/%.o : src/%.c Makefile | ${build_dir}
	$(strip ${COMPILE.C})

%.S : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
%.S : src/%.cc Makefile | ${build_dir}
	$(strip ${COMPILE.S})

%.I : src/%.cc
	$(strip ${PREPROCESS.CXX})

${build_dir} :
	mkdir -p $@

rtags : clean
	${MAKE} -nk all | rtags-rc -c -; true

clean :
	rm -rf ${build_dir} ${exes}

env :
	env | sort --ignore-case

versions:
	${MAKE} --version | head -n1
	${CXX} --version | head -n1

.PHONY : env versions rtags run_benchmarks clean all run_%
