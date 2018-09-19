# time make -rC ~/src/atomic_queue -j8
# time make -rC ~/src/atomic_queue -j8 BUILD=release run_benchmarks

SHELL := /bin/bash
BUILD := debug
build_dir := ${CURDIR}/${BUILD}

CXX := g++
CC := gcc
LD := g++

CXXFLAGS.debug := -O0 -g -fstack-protector-all -fno-omit-frame-pointer
CXXFLAGS.release := -O3 -DNDEBUG
CXXFLAGS := -std=gnu++1z -march=native -pthread -W{all,extra,error,inline} -g -fmessage-length=0 ${CXXFLAGS.${BUILD}}

LDFLAGS.debug :=
LDFLAGS.release :=
LDFLAGS := -g -pthread ${LDFLAGS.${BUILD}}
LDLIBS :=

COMPILE.CXX = ${CXX} -c -o $@ ${CPPFLAGS} -MD -MP ${CXXFLAGS} $<
LINK.EXE = ${LD} -o $@ $(LDFLAGS) $(filter-out Makefile,$^) $(LDLIBS)
LINK.SO = ${LD} -shared -o $@ $(LDFLAGS) $(filter-out Makefile,$^) $(LDLIBS)
LINK.A = ${AR} rcsT $@ $(filter-out Makefile,$^)

all : ${build_dir}/benchmarks

${build_dir} :
	mkdir $@

# ${build_dir}/test : ${build_dir}/libmylib.a
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
	/usr/bin/time --verbose $<
	# sudo chrt -f 50 time --verbose $<

${build_dir}/%.o : %.cc Makefile | ${build_dir}
	$(strip ${COMPILE.CXX})

clean :
	rm -rf ${build_dir}

governor_userspace:
	sudo cpupower frequency-set --related --governor userspace
	sudo cpupower frequency-set --related --freq 5GHz
	sudo cpupower frequency-info

governor_ondemand:
	sudo cpupower frequency-set --related --governor ondemand
	sudo cpupower frequency-info

.PHONY : run_benchmarks clean all run_% governor_userspace governor_ondemand
