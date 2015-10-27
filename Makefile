.PHONY: all clean

CXX=g++
INCLUDES=-Iekg/xg -Iekg/xg/sdsl-lite/build/include -Ibenedictpaten/sonLib/C/inc
CXXFLAGS=-O3 -std=c++11 -fopenmp -g $(INCLUDES)
LDSEARCH=-Lxg
LDFLAGS=-lm -lpthread $(LDSEARCH)
LIBXG=ekg/xg/libxg.a
LIBPROTOBUF=ekg/xg/stream/protobuf/libprotobuf.a
LIBSDSL=ekg/xg/sdsl-lite/build/lib/libsdsl.a

all: main

$(LIBSDSL): $(LIBXG)

$(LIBPROTOBUF): $(LIBXG)

$(LIBXG):
	cd ekg/xg && $(MAKE) libxg.a

# Needs XG to be built for the protobuf headers
main.o: $(LIBXG)

main: main.o $(LIBXG) $(LIBSDSL)
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f main
	rm -f *.o
	cd ekg/xg && $(MAKE) clean
