# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# Usage examples (assuming this directory is ~/src/atomic_queue):
#   time make -rC ~/src/atomic_queue -j8
#   time make -rC ~/src/atomic_queue -j8 run_benchmarks
#   time make -rC ~/src/atomic_queue -j8 TOOLSET=clang run_benchmarks
#   time make -rC ~/src/atomic_queue -j8 BUILD=debug run_tests
#   time make -rC ~/src/atomic_queue -j8 BUILD=sanitize TOOLSET=clang run_tests
# Additional CPPFLAGS, CXXFLAGS, CFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.

SHELL := /bin/bash

BUILD := release
TOOLSET := gcc

build_dir := ${CURDIR}/build/${BUILD}/${TOOLSET}
build_env := ${build_dir}/.env
rebuild := ${build_dir}/.rebuild

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
cxxflags.gcc := -pthread -march=native -std=gnu++14 -W{all,extra,error,no-{maybe-uninitialized,unused-variable,unused-function,unused-local-typedefs,error=array-bounds}} -g -fmessage-length=0 ${cxxflags.gcc.${BUILD}}
ldflags.gcc.sanitize := ${ldflags.gcc.release} -fsanitize=thread
ldflags.gcc := ${ldflags.gcc.${BUILD}}

cflags.gcc := -pthread -march=native -W{all,extra} -g -fmessage-length=0 ${cxxflags.gcc.${BUILD}}

cxxflags.clang.debug := -O0 -fstack-protector-all
cxxflags.clang.release := -O3 -mtune=native -ffast-math -falign-functions=64 -DNDEBUG
cxxflags.clang.sanitize := ${cxxflags.clang.release} -fsanitize=thread
cxxflags.clang := -stdlib=libstdc++ -pthread -march=native -std=gnu++14 -W{all,extra,error,no-{unused-variable,unused-function,unused-local-typedefs}} -g -fmessage-length=0 ${cxxflags.clang.${BUILD}}
ldflags.clang.sanitize := ${ldflags.clang.release} -fsanitize=thread
ldflags.clang := -stdlib=libstdc++ ${ldflags.clang.${BUILD}}

# Additional CPPFLAGS, CXXFLAGS, CFLAGS, LDLIBS, LDFLAGS can come from the command line, e.g. make CPPFLAGS='-I<my-include-dir>', or from environment variables.
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
COMPILE.S = ${CXX} -o- -S -fverbose-asm -masm=intel ${cppflags} ${cxxflags} $(abspath $<) | c++filt | egrep -v '^[[:space:]]*\.(loc|cfi|L[A-Z])' > $@
PREPROCESS.CXX = ${CXX} -o $@ -E ${cppflags} ${cxxflags} $(abspath $<)
COMPILE.C = ${CC} -o $@ -c ${cppflags} ${cflags} -MD -MP $(abspath $<)
LINK.EXE = ${LD} -o $@ $(ldflags) $(filter-out ${rebuild},$^) $(ldlibs)
LINK.SO = ${LD} -o $@ -shared $(ldflags) $(filter-out ${rebuild},$^) $(ldlibs)
LINK.A = ${AR} rscT $@ $(filter-out ${rebuild},$^)

exes := benchmarks tests example

all : ${exes}

${exes} : % : ${build_dir}/%
	ln -sf ${<:${CURDIR}/%=%}

benchmarks_src := benchmarks.cc cpu_base_frequency.cc huge_pages.cc
${build_dir}/benchmarks : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/benchmarks : ldlibs += ${ldlibs.tbb} ${ldlibs.moodycamel} ${ldlibs.xenium} -ldl
${build_dir}/benchmarks : ${benchmarks_src:%.cc=${build_dir}/%.o} ${rebuild} | ${build_dir}
	$(strip ${LINK.EXE})
-include ${benchmarks_src:%.cc=${build_dir}/%.d}

tests_src := tests.cc
${build_dir}/tests : cppflags += ${boost_unit_test_framework_inc} -DBOOST_TEST_DYN_LINK=1
${build_dir}/tests : ldlibs += -lboost_unit_test_framework
${build_dir}/tests : ${tests_src:%.cc=${build_dir}/%.o} ${rebuild} | ${build_dir}
	$(strip ${LINK.EXE})
-include ${tests_src:%.cc=${build_dir}/%.d}

example_src := example.cc
${build_dir}/example : ${example_src:%.cc=${build_dir}/%.o} ${rebuild} | ${build_dir}
	$(strip ${LINK.EXE})
-include ${example_src:%.cc=${build_dir}/%.d}

${build_dir}/%.so : cxxflags += -fPIC
${build_dir}/%.so : ${rebuild} | ${build_dir}
	$(strip ${LINK.SO})

${build_dir}/%.a : ${rebuild} | ${build_dir}
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

${build_dir}/%.o : src/%.cc ${rebuild} | ${build_dir}
	$(strip ${COMPILE.CXX})

${build_dir}/%.o : src/%.c ${rebuild} | ${build_dir}
	$(strip ${COMPILE.C})

${build_dir}/%.S : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/%.S : src/%.cc ${rebuild} | ${build_dir}
	$(strip ${COMPILE.S})

${build_dir}/%.I : cppflags += ${cppflags.tbb} ${cppflags.moodycamel} ${cppflags.xenium}
${build_dir}/%.I : src/%.cc ${rebuild} | ${build_dir}
	$(strip ${PREPROCESS.CXX})

${build_dir} :
	mkdir -p $@

rtags :
	${MAKE} --always-make --just-print all | { rtags-rc -c -; true; }

clean :
	rm -rf ${build_dir} ${exes}

env :
	env | sort --ignore-case

head1 := awk 'FNR<2 {print}' # `make | head -n1` fails when `head` closes its stdin early. Use `awk` to keep reading stdin till EOF instead of `head`.
versions:
	${MAKE} --version | ${head1}
	${CXX} --version | ${head1}

# Track toolset versions and build flag values which may depend on the environment.
build_env_text := printf "%s\n" $(foreach exe,${CXX} ${LD} ${AR},"$(shell ${exe} --version |& ${head1})") ${cppflags} ${cxxflags} ${ldflags} ${ldlibs} ${cppflags.tbb} ${ldlibs.tbb} ${cppflags.moodycamel} ${ldlibs.moodycamel} ${cppflags.xenium} ${ldlibs.xenium}
${build_env} : Makefile $(shell cmp --quiet <(${build_env_text}) ${build_env} || echo update_build_env) | ${build_dir}
	${build_env_text} >$@

# Trigger rebuild when Makefile or build environment change.
${rebuild} : Makefile ${build_env} | ${build_dir}
	@[[ ! -f $@ ]] || { u="$?"; echo "Rebuild is triggered by changes in $${u// /, }."; }
	touch $@

.PHONY : update_build_env env versions rtags run_benchmarks clean all run_%
