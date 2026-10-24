// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026, Amit Gefen

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "SimpleLogger.hpp"

// Date & Time prefix (Example)
static std::wstring DateTimePrefix() {
  const auto now{std::chrono::system_clock::now()};
  const auto time{std::chrono::floor<std::chrono::seconds>(now)};

  return std::format(L"{:%FT%TZ} ", time);
}

// A failure to open the log file is an expected, recoverable condition, so it
// is reported as a value instead of an exception. Allocation failures can still
// throw, which is why the caller keeps a try/catch as a backstop.
static std::expected<void, std::string> ConfigureLogger() {
  auto out_stream{
      std::make_unique<std::wofstream>("SimpleLogger.log", std::ios::app),
  };

  if (out_stream->fail()) {
    return std::unexpected(std::string{"failed to open file"});
  }

  amitgdev::SimpleLogger::SetOstream(std::move(out_stream));
  amitgdev::SimpleLogger::SetPrefixList(
      std::vector<amitgdev::SimpleLogger::PrefixFunction>{DateTimePrefix});

  return {};
}

int main() {
  try {
    if (const auto result{ConfigureLogger()}; !result) {
      std::println(stderr, "failed to configure logger: {}", result.error());
      return EXIT_FAILURE;
    }
  } catch (const std::exception& error) {
    // fputs instead of std::println: the error path must not throw again.
    std::fputs("caught exception: ", stderr);
    std::fputs(error.what(), stderr);
    std::fputs("\n", stderr);
    return EXIT_FAILURE;
  }

  // A function-like macro defined with one parameter can receive an empty
  // argument.
  amitgdev::LOG() << 10;
  amitgdev::LOG(amitgdev::DEBUG) << L"Line " << 1;
  amitgdev::LOG(amitgdev::DEBUG) << L"Line " << 2;
  amitgdev::LOG(amitgdev::DEBUG) << L"Pi = " << std::numbers::pi;
  amitgdev::LOG(amitgdev::DEBUG) << L"Line " << L"End";
  return EXIT_SUCCESS;
}
