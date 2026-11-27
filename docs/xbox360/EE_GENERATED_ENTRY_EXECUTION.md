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
