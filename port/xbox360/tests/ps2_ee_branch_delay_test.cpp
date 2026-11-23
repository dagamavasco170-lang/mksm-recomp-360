#include "ps2_ee_branch_delay.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;

int main()
{
    CpuState cpu{};
    BranchDelayState branch{};

    assert(relativeBranchTarget(0x1000u, 0x0002u) == 0x100Cu);
    assert(relativeBranchTarget(0x1000u, 0xFFFEu) == 0x0FFCu);

    const bool branchInstructionWasDelay = branch.inDelaySlot();
    branch.scheduleBranch(0x2000u);
    branch.finishInstruction(cpu, branchInstructionWasDelay);
    assert(cpu.pc == 4u && cpu.retiredInstructions == 1u);
    assert(branch.inDelaySlot());

    // The delay-slot instruction retires normally, then redirects the PC.
    const bool delayInstructionWasDelay = branch.inDelaySlot();
    branch.finishInstruction(cpu, delayInstructionWasDelay);
    assert(cpu.pc == 0x2000u && cpu.retiredInstructions == 2u);
    assert(!branch.inDelaySlot());

    // No scheduled branch: ordinary instruction advances by four.
    const bool ordinaryWasDelay = branch.inDelaySlot();
    branch.finishInstruction(cpu, ordinaryWasDelay);
    assert(cpu.pc == 0x2004u && cpu.retiredInstructions == 3u);
}
