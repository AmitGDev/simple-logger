/*
    Sink.hpp
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

#ifndef AMITGDEV_SINK_
#define AMITGDEV_SINK_

#include <memory>
#include <mutex>
#include <ostream>
#include <string_view>
#include <utility>

namespace amitgdev {

// A Sink is the destination of finished log lines (file, socket, debugger...).
// It exists so that "where a line goes" and "when it is written" can vary
// independently of how the line is formatted. Async, fan-out and filtering
// behaviors are meant to be decorators implementing this same interface.
//
// Threading contract: Write() and Flush() may be called concurrently from any
// thread. Each implementation picks its own strategy (lock, queue, none), so a
// caller never needs external synchronization.
//
// Failure contract: both functions are noexcept and report failure by return
// value. A logging destination failing (disk full, peer gone) is routine and
// must never disturb the code that is being logged.
class Sink {
 public:
  Sink() = default;
  virtual ~Sink() = default;

  // Polymorphic base: copying or moving through a base reference would slice.
  Sink(const Sink&) = delete;
  Sink& operator=(const Sink&) = delete;
  Sink(Sink&&) = delete;
  Sink& operator=(Sink&&) = delete;

  // `line` is one complete log record WITHOUT a terminator; the sink owns the
  // framing (newline, datagram boundary...). Returns false if the line could
  // not be delivered.
  virtual bool Write(std::wstring_view line) noexcept = 0;

  // Pushes buffered data towards the destination. Returns false on failure.
  virtual bool Flush() noexcept = 0;
};

// Adapts an existing std::wostream, which keeps today's SetOstream() usage
// possible after the logger switches to sinks.
class OStreamSink final : public Sink {
 public:
  // shared_ptr: a sink may be swapped out while another thread is still inside
  // Write(); shared ownership keeps the stream alive for that call.
  explicit OStreamSink(std::shared_ptr<std::wostream> stream)
      : stream_(std::move(stream)) {}

  bool Write(std::wstring_view line) noexcept override {
    // The lock covers the line and its terminator together, so two threads can
    // never interleave inside a record. It cannot protect against other
    // writers of the same stream that bypass this sink.
    const std::lock_guard lock(mutex_);

    if (stream_ == nullptr) {
      return false;
    }

    // A stream with an exception mask, or a throwing streambuf, must not be
    // able to propagate out of a noexcept function (that would terminate).
    try {
      stream_->write(line.data(), static_cast<std::streamsize>(line.size()));
      stream_->put(L'\n');

      return stream_->good();
    } catch (...) {
      return false;
    }
  }

  bool Flush() noexcept override {
    const std::lock_guard lock(mutex_);

    if (stream_ == nullptr) {
      return false;
    }

    try {
      stream_->flush();

      return stream_->good();
    } catch (...) {
      return false;
    }
  }

 private:
  std::mutex mutex_;
  std::shared_ptr<std::wostream> stream_;
};

}  // namespace amitgdev

#endif  // AMITGDEV_SINK_
