# IPC_PROTOCOL.md — versioned local protocol (v1, JSON in prototype)

Families: Hello, HelloAck (protocol version, build id, features); StartRuntime, StartAccepted; StopRuntime, StopAccepted; RuntimeState; HardwareInfo; LogEvent; ProgressEvent; ShaderEvent; FrameStats; DeviceLost; UserActionResult.

Rules: version checked at handshake; reject unknown required fields + invalid enums; cap message size (64 KiB) and queue length (512); no memory-content leaks; current-user ACL on real pipe (in-page bus: no network); never accept remote commands; fuzz tests for malformed, truncated, oversized.

Prototype: window.SB_IPC = { VERSION: 1, validate, roundTripTest, fuzzTest }. Named-pipe framing (length-prefix + ACL) is implemented only in the C++ tree; the web bus preserves identical semantics and versioning so the UI logic ports directly.
