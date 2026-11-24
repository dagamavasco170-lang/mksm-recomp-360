#include "ps2_ee_test_program.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

int main()
{
    const auto trace = ps2::xbox360::ee::test_program::runDeterministicTrace();
    assert(trace.passed);
    std::printf("MKSM_EE_TRACE signature=%08X%08X pc=%08X retired=%u\n",
                static_cast<unsigned>(trace.signature >> 32u),
                static_cast<unsigned>(trace.signature),
                static_cast<unsigned>(trace.pc),
                static_cast<unsigned>(trace.retiredInstructions));
    return trace.passed ? 0 : 1;
}
