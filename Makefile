# time make -rC ~/src/atomic_queue -j8
# time make -rC ~/src/atomic_queue -j8 BUILD=release run_benchmarks

SHELL := /bin/bash
BUILD := debug
build_dir := ${CURDIR}/${BUILD}

CXX := g++
CC := gcc
LD := g++

CXXFLAGS.debug := -O0 -fstack-protector-all -fno-omit-frame-pointer
CXXFLAGS.release := -O3 -DNDEBUG
CXXFLAGS := -std=gnu++14 -march=native -pthread -W{all,extra,error,inline} -g -fmessage-length=0 ${CXXFLAGS.${BUILD}}

CPPFLAGS :=

LDFLAGS.debug :=
LDFLAGS.release :=
LDFLAGS := -g -pthread ${LDFLAGS.${BUILD}}
LDLIBS :=

-include local.mk

COMPILE.CXX = ${CXX} -c -o $@ ${CPPFLAGS} -MD -MP ${CXXFLAGS} $<
LINK.EXE = ${LD} -o $@ $(LDFLAGS) $(filter-out Makefile,$^) $(LDLIBS)
LINK.SO = ${LD} -shared -o $@ $(LDFLAGS) $(filter-out Makefile,$^) $(LDLIBS)
LINK.A = ${AR} rcsT $@ $(filter-out Makefile,$^)

all : ${build_dir}/benchmarks

${build_dir} :
	mkdir $@

${build_dir}/libatomic_queue.a : ${build_dir}/cpu_base_frequency.o

${build_dir}/benchmarks : ${build_dir}/libatomic_queue.a
${build_dir}/benchmarks : ${build_dir}/benchmarks.o Makefile | ${build_dir}
	$(strip ${LINK.EXE})
-include ${build_dir}/benchmarks.d

${build_dir}/%.so : CXXFLAGS += -fPIC
${build_dir}/%.so : Makefile | ${build_dir}
	$(strip ${LINK.SO})

${build_dir}/%.a : Makefile | ${build_dir}
	$(strip ${LINK.A})

run_benchmarks : ${build_dir}/benchmarks
	@echo "---- running $< ----"
	# /usr/bin/time --verbose $<
	sudo chrt -f 50 time --verbose $<

${build_dir}/%.o : %.cc Makefile | ${build_dir}
	$(strip ${COMPILE.CXX})

clean :
	rm -rf ${build_dir}

governor_performance:
	sudo cpupower frequency-set --related --governor performance
	sudo cpupower frequency-info

governor_powersave:
	sudo cpupower frequency-set --related --governor powersave
	sudo cpupower frequency-info

.PHONY : run_benchmarks clean all run_% governor_userspace governor_ondemand
