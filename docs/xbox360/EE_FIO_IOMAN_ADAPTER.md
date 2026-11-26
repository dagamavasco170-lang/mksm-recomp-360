# Generated EE direct FileIO routing

The recompiler's function-table emitter now routes direct FileIO aliases to
Xbox ABI shims when `PS2X_GENERATED_RUNTIME_ABI` is enabled. Desktop builds
retain their original generated function targets.

## Routed symbols

| PS2 symbol | Xbox ABI target |
|---|---|
| `fioOpen`, `open`, `sceOpen` | `ps2XboxFioOpen` |
| `fioRead`, `read`, `sceRead` | `ps2XboxFioRead` |
| `fioLseek`, `lseek`, `sceLseek` | `ps2XboxFioLseek` |
| `fioClose`, `close`, `sceClose` | `ps2XboxFioClose` |
| `fioWrite`, `write`, `sceWrite` | `ps2XboxFioWrite` (returns `-EACCES`) |

The shims consume EE ABI registers (`a0-a2`), return in `v0`, and pass guest
path and buffer addresses through `PS2Runtime::dispatchIomanEe` for bounded
RDRAM validation. IOMAN owns descriptor allocation and close.

Only read-only open is accepted. Write calls and unsupported open flags fail
closed instead of being silently downgraded. Desktop table targets are unchanged.

## Remaining limitations

- Xbox shims bypass desktop `fioRead` VAG accumulation/audio transfer side effects.
- The IOMAN retail path mapping still needs runtime disc-path correlation.
- Write/create/truncate operations are not supported.
- Physical Xbox execution and a real generated-game build have not been verified
  in this environment.
