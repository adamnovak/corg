.PHONY: all clean

CXX=g++
INCLUDES=-Iekg/vg -Iekg/vg/xg -Iekg/vg/xg/sdsl-lite/build/include -Ibenedictpaten/sonLib/C/inc
CXXFLAGS=-O3 -std=c++11 -fopenmp -g $(INCLUDES)
LDSEARCH=-Lekg/vg -Lekg/vg/xg -Lekg/vg/xg/sdsl-lite/build/lib -Lekg/vg/xg/sdsl-lite/build/external/libdivsufsort/lib
LDFLAGS=-lm -lpthread -lz -ldivsufsort -ldivsufsort64 $(LDSEARCH)
LIBVG=ekg/vg/libvg.a
LIBPROTOBUF=ekg/vg/xg/stream/protobuf/libprotobuf.a
LIBSDSL=ekg/vg/xg/sdsl-lite/build/lib/libsdsl.a
LIBPINCHESANDCACTI=benedictpaten/sonLib/lib/stPinchesAndCacti.a
LIBSONLIB=benedictpaten/sonLib/lib/sonLib.a

all: main

$(LIBSDSL): $(LIBXG)

$(LIBPROTOBUF): $(LIBXG)

$(LIBVG):
	cd ekg/vg && $(MAKE) libvg.a
	
# This builds out to the sonLib lib directory for some reason
$(LIBPINCHESANDCACTI): $(LIBSONLIB)
	cd benedictpaten/pinchesAndCacti && $(MAKE)

$(LIBSONLIB):
	cd benedictpaten/sonLib && $(MAKE)

# Needs XG to be built for the protobuf headers
main.o: $(LIBXG)

main: main.o embeddedGraph.o $(LIBVG) $(LIBSDSL) $(LIBPINCHESANDCACTI) $(LIBSONLIB) $(LIBPROTOBUF)
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f main
	rm -f *.o
	cd ekg/vg && $(MAKE) clean
