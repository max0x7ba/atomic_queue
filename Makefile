# time make -rC ~/src/atomic_queue -j8 run_benchmarks
# time make -rC ~/src/atomic_queue -j8 BUILD=debug run_benchmarks
# time make -rC ~/src/atomic_queue -j8 TOOLSET=gcc-8 run_benchmarks
# time make -rC ~/src/atomic_queue -j8 TOOLSET=gcc-8 run_tests

SHELL := /bin/bash
BUILD := release

TOOLSET := gcc
build_dir := ${CURDIR}/${BUILD}/${TOOLSET}

cxx.gcc := g++
cc.gcc := gcc
ld.gcc := g++
ar.gcc := gcc-ar

cxx.gcc-8 := g++-8
cc.gcc-8 := gcc-8
ld.gcc-8 := g++-8
ar.gcc-8 := gcc-ar-8

cxx.clang := clang++
cc.clang := clang
ld.clang := clang++
ar.clang := ar

cxx.clang-7 := clang++-7
cc.clang-7 := clang-7
ld.clang-7 := clang++-7
ar.clang-7 := ar

CXX := ${cxx.${TOOLSET}}
CC := ${cc.${TOOLSET}}
LD := ${ld.${TOOLSET}}
AR := ${ar.${TOOLSET}}

cxxflags.gcc.debug := -Og -fstack-protector-all -fno-omit-frame-pointer # -D_GLIBCXX_DEBUG
cxxflags.gcc.release := -O3 -march=native -ffast-math -falign-{functions,loops}=32 -DNDEBUG
cxxflags.gcc := -pthread -std=gnu++14 -march=native -W{all,extra,error} -g -fmessage-length=0 ${cxxflags.gcc.${BUILD}}
cxxflags.gcc-8 := ${cxxflags.gcc}

cflags.gcc := -pthread -march=native -W{all,extra} -g -fmessage-length=0 ${cxxflags.gcc.${BUILD}}
cflags.gcc-8 := ${cflags.gcc}

cxxflags.clang.debug := -O0 -fstack-protector-all
cxxflags.clang.release := -O3 -march=native -ffast-math -DNDEBUG
cxxflags.clang := -pthread -std=gnu++14 -march=native -W{all,extra,error} -g -fmessage-length=0 ${cxxflags.clang.${BUILD}}
cxxflags.clang-7 := ${cxxflags.clang}

cxxflags := ${cxxflags.${TOOLSET}} ${CXXFLAGS}
cflags := ${cflags.${TOOLSET}} ${CFLAGS}

cppflags := ${CPPFLAGS}

ldflags.debug :=
ldflags.release :=
ldflags := -fuse-ld=gold -pthread -g ${ldflags.${BUILD}} ${ldflags.${TOOLSET}}
ldlibs := -lrt ${LDLIBS}

COMPILE.CXX = ${CXX} -c -o $@ ${cppflags} -MD -MP ${cxxflags} $(abspath $<)
COMPILE.S = ${CXX} -S -masm=intel -o- ${cppflags} ${cxxflags} $(abspath $<) | c++filt > $@
PREPROCESS.CXX = ${CXX} -E -o $@ ${cppflags} ${cxxflags} $(abspath $<)
COMPILE.C = ${CC} -c -o $@ ${cppflags} -MD -MP ${cflags} $(abspath $<)
LINK.EXE = ${LD} -o $@ $(ldflags) $(filter-out Makefile,$^) $(ldlibs)
LINK.SO = ${LD} -shared -o $@ $(ldflags) $(filter-out Makefile,$^) $(ldlibs)
LINK.A = ${AR} rscT $@ $(filter-out Makefile,$^)

all : ${build_dir}/benchmarks ${build_dir}/tests

${build_dir} :
	mkdir -p $@

${build_dir}/libatomic_queue.a : ${build_dir}/cpu_base_frequency.o

${build_dir}/benchmarks : ${build_dir}/libatomic_queue.a
${build_dir}/benchmarks : ${build_dir}/benchmarks.o Makefile | ${build_dir}
	$(strip ${LINK.EXE})
-include ${build_dir}/benchmarks.d

${build_dir}/tests : ldlibs += -lboost_unit_test_framework
${build_dir}/tests : ${build_dir}/tests.o Makefile | ${build_dir}
	$(strip ${LINK.EXE})
-include ${build_dir}/tests.d

${build_dir}/%.so : cxxflags += -fPIC
${build_dir}/%.so : Makefile | ${build_dir}
	$(strip ${LINK.SO})

${build_dir}/%.a : Makefile | ${build_dir}
	$(strip ${LINK.A})

run_benchmarks : ${build_dir}/benchmarks
	@echo "---- running $< ----"
	sudo chrt -f 50 $<
#	sudo chrt -f 50 perf stat -d $<

run_tests : ${build_dir}/tests
	@echo "---- running $< ----"
	$<

${build_dir}/%.o : %.cc Makefile | ${build_dir}
	$(strip ${COMPILE.CXX})

${build_dir}/%.o : %.c Makefile | ${build_dir}
	$(strip ${COMPILE.C})

%.S : %.cc Makefile | ${build_dir}
	$(strip ${COMPILE.S})

%.I : %.cc
	$(strip ${PREPROCESS.CXX})

rtags : clean
	${MAKE} -nk | rc -c -

clean :
	rm -rf ${build_dir}

.PHONY : rtags run_benchmarks clean all run_%
