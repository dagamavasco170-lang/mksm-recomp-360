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

## Evidence-backed mapping audit

Once an inventory JSON exists, compare it to a hand-reviewed mapping file without
assigning semantics to IDs automatically.

Example mapping file (illustrative only; do not copy IDs without verification):

```json
{
  "schema": "mksm-ee-syscall-mapping-v1",
  "mappings": [
    {
      "syscall_id": "0x123",
      "command": "Read",
      "evidence": "Reviewed dispatcher entry and generated call site"
    }
  ]
}
```

Commands are restricted to `Open`, `Read`, `Write`, `Lseek`, and `Close`.
Every mapping requires a non-empty evidence description.

```sh
python tools/ee_syscall_inventory.py --json generated/*.cpp > syscall-inventory.json
python tools/ee_syscall_mapping_audit.py --json syscall-inventory.json --mapping verified-map.json
```

The report separates verified mappings, unresolved observed IDs, and mapping entries
that are not present in the inventory. The mapping audit is intentionally
conservative: evidence descriptions are human assertions and should point to the
actual dispatcher/table or reviewed generated call-site.
