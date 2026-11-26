# EE direct FileIO adapter

`EeFioIomanAdapter` is the Xbox-side seam for the PS2 EE direct `fioOpen`,
`fioRead`, `fioLseek`, and `fioClose` operations. It forwards through
`PS2Runtime::dispatchIomanEe`, so EE guest addresses are checked and translated
by the existing runtime ABI before the IOMAN dispatcher touches memory.

## Current support

- Open/read/seek/close, with descriptor ownership delegated to IOMAN.
- Read-only open (`FIO_O_RDONLY`) only.
- Guest EE pointers for paths and read buffers.
- Invalid seek origins rejected.

Write/create/append/truncate flags are rejected with `-EACCES`; they are not
silently mapped to a read-only operation. The adapter does not implement VAG
audio accumulation/side effects performed by the desktop `fioRead` implementation.

## Integration boundary

This is a standalone, tested adapter header. It does **not yet replace** the
desktop `ps2_syscalls::fio*` implementations. The next integration change must
add an Xbox build/runtime hook at those direct-call functions, preserving the
desktop host-file fallback and VAG/audio behavior. The adapter itself does not
guess PS2 runtime object internals or generated call-site bindings.

## Test

`port/xbox360/tests/ps2_fio_ioman_adapter_test.cpp` verifies read-only open,
read, seek, close, invalid flags, and descriptor invalidation using a temporary
fixture.
