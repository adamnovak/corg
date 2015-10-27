.PHONY: all clean

CXX=g++
INCLUDES=-Iekg/xg -Iekg/xg/sdsl-lite/build/include -Ibenedictpaten/sonLib/C/inc
CXXFLAGS=-O3 -std=c++11 -fopenmp -g $(INCLUDES)
LDSEARCH=-Lxg
LDFLAGS=-lm -lpthread $(LDSEARCH)
LIBXG=ekg/xg/libxg.a
LIBPROTOBUF=ekg/xg/stream/protobuf/libprotobuf.a
LIBSDSL=ekg/xg/sdsl-lite/build/lib/libsdsl.a
LIBPINCHESANDCACTI=benedictpaten/sonLib/lib/stPinchesAndCacti.a
LIBSONLIB=benedictpaten/sonLib/lib/sonLib.a

all: main

$(LIBSDSL): $(LIBXG)

$(LIBPROTOBUF): $(LIBXG)

$(LIBXG):
	cd ekg/xg && $(MAKE) libxg.a
	
# This builds out to the sonLib lib directory for some reason
$(LIBPINCHESANDCACTI): $(LIBSONLIB)
	cd benedictpaten/pinchesAndCacti && $(MAKE)

$(LIBSONLIB):
	cd benedictpaten/sonLib && $(MAKE)

# Needs XG to be built for the protobuf headers
main.o: $(LIBXG)

main: main.o $(LIBXG) $(LIBSDSL) $(LIBPINCHESANDCACTI) $(LIBSONLIB)
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f main
	rm -f *.o
	cd ekg/xg && $(MAKE) clean
