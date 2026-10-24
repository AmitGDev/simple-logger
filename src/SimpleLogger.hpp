/*
    SimpleLogger.hpp
    Copyright (c) 2024-2026, Amit Gefen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef AMITGDEV_SIMPLE_LOGGER_
#define AMITGDEV_SIMPLE_LOGGER_

#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Sink.hpp"

namespace amitgdev {

// Macros for logging__

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG(severity) SimpleLogger(severity)

// Severities:
#define DEBUG SimpleLogger::Severity::kDebug
#define INFO SimpleLogger::Severity::kInfo
#define WARNING SimpleLogger::Severity::kWarning
#define ERROR SimpleLogger::Severity::kError
#define CRITICAL SimpleLogger::Severity::kCritical

// __Macros for logging

// SimpleLogger class provides a simple logging utility for C++23 programs.
// It formats one log line per instance and hands it to a Sink, which decides
// where and when the line is actually written (file, debugger, network, async
// queue...). The logger itself knows nothing about the destination.

class SimpleLogger final {
 public:
  // Prefix function signature
  using PrefixFunction = std::function<std::wstring()>;

  enum class Severity : std::uint8_t {
    kDebug,
    kInfo,
    kWarning,
    kError,
    kCritical,
  };  // Bounds: 0-4.

  // std::wstring is not a literal type under MSVC (it allocates), so it cannot
  // live in a constexpr object. std::wstring_view over string literals can.
  static constexpr std::array<std::wstring_view, 5> kSeverityMap{
      L"DEBUG", L"INFO", L"WARNING", L"ERROR", L"CRITICAL",
  };

  // Constructor
  // explicit: every parameter is defaulted, so without it a bare Severity
  // would silently convert into a SimpleLogger.
  explicit SimpleLogger(Severity severity = Severity::kDebug) {
    const std::shared_lock lock(mutex_);

    if (active_sink_ == nullptr) {
      return;
    }

    // In the context of a logging utility, it's generally not a good idea to
    // throw exceptions because it could lead to the termination of the entire
    // process if not caught. Logging should be a non-intrusive operation and
    // should not affect the normal flow of the program.
    try {
      // Pin the sink for the lifetime of this log line. Without this, a
      // concurrent SetSink() could release the sink while the destructor is
      // about to write to it.
      sink_ = active_sink_;

      // Emplaced inside the try block because constructing a stream allocates
      // and may throw. An empty buffer_ doubles as the "this line is inactive"
      // state, so no separate flag can fall out of sync with it.
      buffer_.emplace();

      for (const auto& prefix : prefix_function_list_) {
        *buffer_ << prefix();
      }

      *buffer_ << kSeverityMap.at(std::to_underlying(severity)) << L": ";
    } catch (const std::exception& e) {
      ReportError(e);
    }
  }

  ~SimpleLogger() {
    // No lock is needed here because only per-instance state is accessed; the
    // pinned sink_ stays valid even if SetSink() replaced the global one.
    if (!buffer_.has_value()) {
      return;
    }

    // The whole line is delivered in a single call so the sink can keep it
    // atomic. The result is deliberately ignored: how to react to a failing
    // destination (drop, retry, reconnect) is the sink's policy, and the
    // logger must never disturb the caller's flow. Write() is noexcept, so
    // nothing can escape this destructor.
    sink_->Write(buffer_->view());
  }

  SimpleLogger(const SimpleLogger&) = delete;
  SimpleLogger& operator=(const SimpleLogger&) = delete;
  SimpleLogger(SimpleLogger&&) = delete;
  SimpleLogger& operator=(SimpleLogger&&) = delete;

  // Operator <<
  // On use of the operator<< function with an argument of a certain type, T is
  // replaced with that type.
  //
  // Intentionally not noexcept: inserting a user-supplied T can throw, and
  // declaring noexcept would turn that into std::terminate. The exception is
  // swallowed here instead, so logging never disturbs the caller's flow.
  template <typename T>
  SimpleLogger& operator<<(const T& value) {
    if (buffer_.has_value()) {
      try {
        // String literals arrive as const wchar_t[N] and must decay to a
        // pointer for the standard stream inserter; this is unavoidable here.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        *buffer_ << value;
      } catch (const std::exception& e) {
        ReportError(e);
      }
    }

    return *this;  // Allow chaining for convenience (optional).
  }

  // Set the destination of all log lines. nullptr disables logging.
  // Takes a shared_ptr so in-flight log lines can keep the previous sink alive.
  static void SetSink(std::shared_ptr<Sink> sink) noexcept {
    const std::lock_guard lock(mutex_);

    // Swap instead of assign so the previous sink is released when the
    // parameter dies, i.e. after the lock is dropped. Destroying a sink may
    // flush or join a thread, which must not happen while holding the lock
    // that every new log line needs.
    active_sink_.swap(sink);
  }

  // Convenience for the common case of logging to an existing wide stream.
  // Kept so existing callers keep working. Not noexcept: wrapping the stream
  // allocates, which is why it is done here, outside the locked section.
  static void SetOstream(std::shared_ptr<std::wostream> out_stream) {
    if (out_stream == nullptr) {
      SetSink(nullptr);

      return;
    }

    SetSink(std::make_shared<OStreamSink>(std::move(out_stream)));
  }

  // Set the prefix list
  // Taken by value so the (potentially throwing) copy happens at the call site,
  // outside the lock, leaving the locked section a non-throwing move.
  static void SetPrefixList(std::vector<PrefixFunction> prefix_list) noexcept {
    const std::lock_guard lock(mutex_);

    prefix_function_list_ = std::move(prefix_list);  // (Take ownership)
  }

 private:
  // Last-resort diagnostics for failures inside the logger itself. Goes to
  // stderr because the configured sink is the thing that may have just failed.
  // Uses C stdio rather than iostreams: it is called from handlers, where it
  // must be provably unable to throw. The trade-off is that the three writes
  // are not atomic relative to other stderr writers.
  static void ReportError(const std::exception& error) noexcept {
    std::fputs("caught exception: ", stderr);
    std::fputs(error.what(), stderr);
    std::fputs("\n", stderr);
  }

  // The sink this line is delivered to (pinned, see constructor).
  std::shared_ptr<Sink> sink_{nullptr};

  // Per-line formatting buffer. Purely local, so formatting touches no shared
  // state and never blocks on I/O; only the final Write() reaches the sink.
  std::optional<std::wostringstream> buffer_;

  // Shared among all instances of the SimpleLogger class across the entire
  // process. Even if there are no instances of the class SimpleLogger in
  // existence, the static member variables will still exist and retain their
  // values until the program terminates.

  // The prefix functions (pointers):
  inline static std::vector<PrefixFunction> prefix_function_list_{};

  // The process-wide destination:
  inline static std::shared_ptr<Sink> active_sink_{nullptr};

  // Synchronizes access to all static member variables:
  // Allows multiple threads to concurrently read shared resources
  // while preventing concurrent writes or read and write operations.
  // Assumes predominantly read operations, predicting negligible
  // impact on performance.
  inline static std::shared_mutex mutex_{};
};

}  // namespace amitgdev

#endif  // AMITGDEV_SIMPLE_LOGGER_
