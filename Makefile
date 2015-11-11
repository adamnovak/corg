.PHONY: all clean

CXX=g++
INCLUDES=-Iekg/vg -Iekg/vg/gssw/src -Iekg/vg/protobuf/build/include -Iekg/vg/gcsa2 -Iekg/vg/cpp -Iekg/vg/sdsl-lite/install/include -Iekg/vg/vcflib/src -Iekg/vg/vcflib -Iekg/vg/vcflib/tabixpp/htslib -Iekg/vg/progress_bar -Iekg/vg/sparsehash/build/include -Iekg/vg/lru_cache -Iekg/vg/fastahack -Iekg/vg/xg -Iekg/vg/xg/sdsl-lite/build/include -Ibenedictpaten/sonLib/C/inc
CXXFLAGS=-O3 -std=c++11 -fopenmp -g $(INCLUDES)
LDSEARCH=-Lekg/vg -Lekg/vg/xg -Lekg/vg/xg/sdsl-lite/build/lib -Lekg/vg/xg/sdsl-lite/build/external/libdivsufsort/lib
LDFLAGS=-lm -lpthread -lz -ldivsufsort -ldivsufsort64 -ljansson $(LDSEARCH)
LIBVG=ekg/vg/libvg.a
LIBXG=ekg/vg/xg/libxg.a
LIBPROTOBUF=ekg/vg/protobuf/libprotobuf.a
LIBSDSL=ekg/vg/sdsl-lite/install/lib/libsdsl.a
LIBPINCHESANDCACTI=benedictpaten/sonLib/lib/stPinchesAndCacti.a
LIBSONLIB=benedictpaten/sonLib/lib/sonLib.a
LIBGSSW=ekg/vg/gssw/src/libgssw.a
LIBSNAPPY=ekg/vg/snappy/libsnappy.a
LIBROCKSDB=ekg/vg/rocksdb/librocksdb.a
LIBHTS=ekg/vg/htslib/libhts.a
LIBGCSA2=ekg/vg/gcsa2/libgcsa2.a
LIBVCFLIB=ekg/vg/vcflib/libvcflib.a
VGLIBS=$(LIBVG) $(LIBXG) $(LIBVCFLIB) $(LIBGSSW) $(LIBSNAPPY) $(LIBROCKSDB) $(LIBHTS) $(LIBGCSA2) $(LIBSDSL) $(LIBPROTOBUF)

all: corg

$(LIBSDSL): $(LIBVG)

$(LIBPROTOBUF): $(LIBVG)

$(LIBVG):
	cd ekg/vg && $(MAKE) libvg.a

# This builds out to the sonLib lib directory for some reason
$(LIBPINCHESANDCACTI): $(LIBSONLIB)
	cd benedictpaten/pinchesAndCacti && $(MAKE)

$(LIBSONLIB):
	cd benedictpaten/sonLib && $(MAKE)

# Needs XG to be built for the protobuf headers
main.o: $(LIBXG)

corg: main.o embeddedGraph.o $(LIBPINCHESANDCACTI) $(LIBSONLIB) $(VGLIBS)
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f corg
	rm -f *.o
	cd ekg/vg && $(MAKE) clean
