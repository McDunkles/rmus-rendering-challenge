//
// Created by dunca on 2025-11-11.
//

#ifndef RENDERINGCHALLENGE_TESTBENCH_H
#define RENDERINGCHALLENGE_TESTBENCH_H

#include <fstream>

#define TB_AOUT_SIG "[TestBench] "

class Testbench {

private:
    std::ifstream pcd_file;

public:

    Testbench() = default;

    void testOctreeFuncs();

};


#endif //RENDERINGCHALLENGE_TESTBENCH_H
