// main.cpp: Main file for core graph merger

#include <iostream>
#include <fstream>
#include <getopt.h>

#include "ekg/xg/xg.hpp"

#include "embeddedGraph.hpp"

// Hack around stupid name mangling issues
extern "C" {
    #include "benedictpaten/pinchesAndCacti/inc/stPinchGraphs.h"
}

void help_main(char** argv) {
    std::cerr << "usage: " << argv[0] << " [options] XGFILE XGFILE" << std::endl
        << "Compute the core graph from two graphs" << std::endl
        << std::endl
        << "options:" << std::endl
        << "    -h, --help           print this help message" << std::endl;
}

int main(int argc, char** argv) {
    
    if(argc == 1) {
        // Print the help
        help_main(argv);
        return 1;
    }
    
    optind = 1; // Start at first real argument
    bool optionsRemaining = true;
    while(optionsRemaining) {
        static struct option longOptions[] = {
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        int optionIndex = 0;

        switch(getopt_long(argc, argv, "h", longOptions, &optionIndex)) {
        // Option value is in global optarg
        case -1:
            optionsRemaining = false;
            break;
        case 'h': // When the user asks for help
        case '?': // When we get options we can't parse
            help_main(argv);
            exit(1);
            break;
        default:
            // TODO: keep track of the option
            std::cerr << "Illegal option" << std::endl;
            exit(1);
        }
    }
    
    if(argc - optind < 2) {
        // We don't have two positional arguments
        // Print the help
        help_main(argv);
        return 1;
    }
    
    // Pull out the XG file names
    std::string xgFile1 = argv[optind++];
    std::string xgFile2 = argv[optind++];
    
    // Open the files
    std::ifstream xgStream1(xgFile1);
    if(!xgStream1.good()) {
        std::cerr << "Could not read " << xgFile1 << std::endl;
        exit(1);
    }
    
    std::ifstream xgStream2(xgFile2);
    if(!xgStream2.good()) {
        std::cerr << "Could not read " << xgFile2 << std::endl;
        exit(1);
    }
    
    // Load up the first XG file
    xg::XG xg1(xgStream1);
    // And the second
    xg::XG xg2(xgStream2);
    
    // Make a thread set
    auto threadset = stPinchThreadSet_construct();
    
    // Add in each xg graph to the thread set
    coregraph::EmbeddedGraph embedding1(xg1, threadset);
    
    stPinchThreadSet_destruct(threadset);
    
    return 0;
}


