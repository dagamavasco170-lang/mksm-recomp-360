# IOMAN trace attachment ordering

`PS2Runtime` retains the optional trace sink even when the dispatcher has not
been attached yet. This allows bootstrap code to create a bounded trace ring and
attach it before wiring the IOMAN dispatcher:

```cpp
IomanTraceRing trace;
PS2Runtime runtime;
runtime.attachIomanTrace(&trace); // safe before dispatcher exists
IomanDispatcher dispatcher(root);
runtime.attachIoman(&dispatcher); // dispatcher inherits the remembered sink
```

The reverse order is also supported. Reattaching either pointer updates the
current dispatcher/sink. Lifetime remains caller-owned: both the trace sink and
dispatcher must outlive their use by the runtime.

This closes an initialization-order hole, but it does not create a full gameplay
runtime entry point. The checked-in smoke harness currently traces its diagnostic
GAMEDATA.WAD open/read/seek/close sequence. A gameplay trace still requires the
recompiled title code to be launched through a runtime that constructs and wires
PS2Runtime + IOMAN; that launcher integration is not yet present in this branch.
