// main.cpp: Main file for core graph merger

#include <iostream>

#include "ekg/xg/xg.hpp"

// Hack around stupid name mangling issues
extern "C" {
    #include "benedictpaten/pinchesAndCacti/inc/stPinchGraphs.h"
}

int main(int argc, char** argv) {
    std::cout << "Hello world!" << std::endl;
    
    xg::XG testxg;
    
    auto threadset = stPinchThreadSet_construct();
    
    stPinchThreadSet_destruct(threadset);
    
    return 0;
}
