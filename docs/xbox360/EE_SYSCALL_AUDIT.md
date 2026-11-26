# EE syscall/FileIO call-site inventory

The generated recompilation output is not currently tracked in the Xbox 360 branch,
so game-specific call-site IDs cannot be asserted from repository sources yet.

## Run the inventory

```sh
python tools/ee_syscall_inventory.py --json path/to/generated/*.cpp
```

The tool reports:
- concrete numeric IDs emitted as `runtime->handleSyscall(rdram, ctx, ID)`;
- direct generated calls to `fioOpen`, `fioRead`, `fioLseek`, `fioClose`, and `fioWrite`;
- source path and line for every observation.

It intentionally does **not** infer that a numeric syscall ID means Open/Read/Seek/Close.
IDs must be verified against the actual call sites, the syscall instruction encoding,
or symbol/dispatcher evidence before binding the Xbox IOMAN.

## Current state

- Runtime EE guest-pointer IOMAN dispatch: implemented and synthetic-tested.
- EE syscall-ID map: implemented; test IDs are synthetic.
- Generated game call-site inventory: not available in tracked sources.
- Real MKSM syscall bindings: pending verified generated-code evidence.
