# Runtime bootstrap composition

`ps2_runtime_bootstrap.h` provides the first owned composition boundary for the
Xbox port's file-I/O path. It constructs and owns, in lifetime-safe order:

1. a bounded metadata-only IOMAN trace ring;
2. the host-disc-root IOMAN dispatcher;
3. the generated-code `PS2Runtime` ABI bridge.

The constructor attaches the trace sink before and after dispatcher creation
through the existing order-independent runtime API. The bootstrap exposes
explicit syscall binding and dispatch. It deliberately does not assign syscall
numbers: the title/build-specific IDs must come from verified executable or
generated-call-site metadata.

The EE RDRAM span is caller-owned and must cover the full 32 MiB guest RDRAM
window. Open/read/lseek/close arguments use EE virtual addresses and are
translated by the existing generated runtime bridge. Unsupported syscall IDs
remain `-ENOSYS`; writes remain denied by the existing read-only policy.

## Scope boundary

This is composition of the implemented IOMAN/file path, not a complete title
launcher or EE execution loop. It does not yet instantiate the desktop
`R5900Context`, generated title entry point, VU/GS pipeline, full IOP module
environment, or game-specific syscall table. No retail game assets are stored
in this repository.

## Focused regression

`tests/ps2_runtime_bootstrap_test.cpp` drives open/read/seek/close through the
EE virtual-address bridge using a temporary fixture file and verifies syscall
mapping, readback, descriptor cleanup, trace count, and unmapped-ID behavior.
The test source is committed but has not been compiled or executed in this
editing environment.
