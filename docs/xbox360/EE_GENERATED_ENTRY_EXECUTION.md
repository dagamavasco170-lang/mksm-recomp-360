# EE generated-entry execution boundary

`PS2RuntimeBootstrap` now owns a 32 MiB EE RDRAM buffer and the narrow
`R5900Context` used by the Xenon generated-code ABI. It exposes:

- `eeRdram()`: mutable/const spans over guest RDRAM;
- `context()`: the generated EE register/PC context;
- `invokeGeneratedEntry(entry)`: invokes one generated function with the
  owned RDRAM, context and runtime bridge;
- `dispatchIomanFromContext(syscallId)`: takes a0-a2 from GPR4-GPR6, routes
  the explicitly supplied verified syscall ID through IOMAN, and writes the
  signed result to v0/GPR2.

The bootstrap does not infer syscall IDs, choose a title entry point, or chain
generated blocks. Those require a verified generated-function map and title
metadata. The narrow generated ABI context is not yet the full R5900 EE state:
COP0, full exception/interrupt scheduling, VU/GS state and instruction timing
are not bridged by this change. The portable interpreter's `ee::CpuState`
and `ee::step` remain a separate execution path rather than being silently
treated as equivalent to generated-code state.

The regression fixture checks the generated-entry call contract, guest memory
ownership, PC/GPR mutation, context-based syscall argument/return registers,
and the existing file-I/O path. The test source has been committed but was not
compiled or executed in this editing environment.


## PC-to-generated-block dispatch

The host function-table emitter now emits a sparse `g_ps2RecompiledBlockEntries` map alongside the legacy dense table. Each row contains a guest EE entry/resume PC selected by the existing function discovery logic and its generated function pointer. Xbox FileIO entries resolve to the same Xbox IOMAN bridge as the dense table.

`RecompiledBlockDispatcher` loads that sparse map and resolves `R5900Context::pc` to one generated function. It executes sequential blocks until the PC is unmapped, a block makes no PC progress, or the caller-supplied block budget is exhausted. The budget is mandatory; no unbounded guest loop is possible through this API. Unknown PCs are reported rather than guessed from address ranges.

The map is only as complete as the generated function list supplied to the emitter. This does not establish that all title code is recompiled, nor does it add EE exceptions, interrupts, COP0, VU/GS scheduling, or cycle accuracy. Tests use synthetic fixture PCs to verify dispatch mechanics, not game execution.


At runtime, after the generated translation unit is linked, call the no-argument `PS2RuntimeBootstrap::registerGeneratedBlocks()` overload to load the emitter-produced sparse table. The span-taking overload remains available for tests and externally supplied verified maps.
