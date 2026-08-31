# SimpleLogger

A small, header-only C++23 thread-safe logger with a streaming API, severity levels, pluggable line prefixes and pluggable output destinations (sinks).

## What it shows

| Feature | API | Mechanism |
|---|---|---|
| Streaming log line | `LOG(DEBUG) << ...` | `SimpleLogger` temporary, one per line |
| Severity levels | `DEBUG`, `INFO`, `WARNING`, `ERROR`, `CRITICAL` | `SimpleLogger::Severity` enum |
| Output destination | `SetSink` | `std::shared_ptr<Sink>` (abstract interface) |
| Output stream shortcut | `SetOstream` | Wraps a `std::wostream` in an `OStreamSink` |
| Line prefixes | `SetPrefixList` | `std::function<std::wstring()>` list |
| Atomic lines | Per-line buffering, one `Write()` per line | `std::wostringstream` + sink-side locking |
| Shared configuration | Static state guarded by a lock | `std::shared_mutex` |

The logger formats a line; a `Sink` decides where and when it is written. `Sink.hpp` defines the interface and `OStreamSink`, an adapter for any `std::wostream`. Other destinations (file, debugger, network) and behaviors (async, fan-out) are meant to be additional `Sink` implementations or decorators.

The demo opens `SimpleLogger.log` in append mode, installs a UTC timestamp prefix and logs a few lines:

```cpp
SimpleLogger::SetOstream(std::move(out_stream));
SimpleLogger::SetPrefixList({DateTimePrefix});

LOG(DEBUG) << L"Pi = " << std::numbers::pi;
```

`SetOstream` is shorthand for installing an `OStreamSink`:

```cpp
SimpleLogger::SetSink(std::make_shared<OStreamSink>(std::move(out_stream)));
```

## Writing a sink

Derive from `Sink` and implement two `noexcept` functions:

```cpp
class MySink final : public Sink {
 public:
  bool Write(std::wstring_view line) noexcept override;  // One record, no terminator.
  bool Flush() noexcept override;
};
```

- `Write` may be called concurrently from any thread; each sink chooses its own strategy (lock, queue, none).
- `line` has no terminator. The sink owns the framing (newline, datagram boundary and so on).
- Both functions report failure by returning `false`. The logger ignores the result, so the policy for a failing destination (drop, retry, reconnect) belongs to the sink.

## Expected output

Each line is the prefixes, the severity, then the message. `LOG()` with no argument defaults to `DEBUG`. Timestamps vary per run:

```text
2026-10-24T09:15:42Z DEBUG: 10
2026-10-24T09:15:42Z DEBUG: Line 1
2026-10-24T09:15:42Z DEBUG: Line 2
2026-10-24T09:15:42Z DEBUG: Pi = 3.14159
2026-10-24T09:15:42Z DEBUG: Line End
```

## Notes

- Each `LOG(...)` statement creates a temporary that buffers the whole line in a local `std::wostringstream`. Formatting touches no shared state and never blocks on I/O. The finished line is handed to the sink in a single `Write()` call when the temporary is destroyed, so lines from concurrent threads do not interleave (`OStreamSink` writes the line and its newline under one lock).
- Logging takes a shared lock, while `SetSink`, `SetOstream` and `SetPrefixList` take an exclusive lock. Concurrent logging does not serialize on the configuration lock, and reconfiguring never races with a log line being built.
- Each log line pins the sink that was active when it was created, so replacing the sink while other threads are logging is safe: in-flight lines finish on the old sink. The previous sink is released after the configuration lock is dropped, so a slow sink destructor cannot stall new log lines.
- Logging never throws into the caller. Failures inside the logger are reported to `stderr`, and if no sink is set (or `SetSink(nullptr)` was called), log statements are silently dropped.
- The prefix list is passed by value, so the copy happens at the call site and the locked section only moves the vector.
- `OStreamSink` serializes only the writers that go through it. Anything else writing to the same stream bypasses its lock.
- The severity macros are short and unprefixed, so they can collide with other headers (for example, `ERROR` in the Windows headers).
- The logger is wide-character based: lines are `std::wstring_view` and string literals are written as `L"..."`. Converting to another encoding (for example UTF-8 for files or sockets) is the job of the sink.

## Requirements

A C++23-compatible compiler and standard library.

## Build and Run

- [Build on Windows](https://github.com/AmitGDev/cpp-ci-template#build-on-windows)
- [Build on Linux](https://github.com/AmitGDev/cpp-ci-template#build-on-linux)

## Code Style

- [C++](https://github.com/AmitGDev/cpp-coding-standard)
- [Python](https://github.com/AmitGDev/cpp-ci-template#python)
- [CMake](https://github.com/AmitGDev/cpp-ci-template#cmake)

## License

MIT License. Copyright © 2024-2026 Amit Gefen.

---

**Last Updated:** 2026-10-24  
**Maintainer:** AmitGDev
