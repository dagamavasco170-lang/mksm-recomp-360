# Xbox 360 IOMAN metadata tracing

The bootstrap IOP file dispatcher exposes an optional `IomanTraceSink`. Attach a
sink when building a host test or runtime integration to collect metadata for
`Open`, `Read`, `Lseek`, and `Close` dispatches.

Trace records include:
- operation enum
- path (only for Open, transient `string_view` valid during the callback)
- descriptor (Open uses its returned descriptor; other operations use the input fd)
- request arguments 1 and 2
- return value (bytes read, new seek offset, descriptor, or negative errno)

The trace never captures file payload bytes. It is optional: pass a null sink or
leave it unset to disable it. The callback should copy the path if it needs to
retain it after the callback. The sink owns storage, formatting, and any
platform-specific output; the dispatcher does not assume a console/debug logger.

Example:
```cpp
struct Trace final : ps2::xbox360::iop::IomanTraceSink {
    void onIomanTrace(const ps2::xbox360::iop::IomanTraceRecord& record) noexcept override {
        // Record metadata only; copy record.path if retaining it.
    }
};
Trace trace;
IomanDispatcher ioman(root, &trace);
```

Host test `ps2_iop_ioman_test` verifies callback events for open/read/seek/close.
A physical Xbox 360 run is still required to capture actual game resource requests.
