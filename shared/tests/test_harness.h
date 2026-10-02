#pragma once

/// Minimal zero-dependency test harness for the headless `shared/`
/// simulation. Deliberately not a framework: the whole point of keeping
/// `shared/` free of raylib is that its tests need nothing but a C++20
/// compiler, so the test target stays buildable in the same environments the
/// server builds in (including the Docker image and the submodule-less CI
/// job).
///
/// Usage:
///
///     TEST(my_case_name) {
///       REQUIRE(setupSucceeded());  // abandons this test on failure
///       CHECK_EQ(actual, expected); // records and keeps going
///     }
///
/// `TEST` self-registers the case; `test_main.cpp` runs everything and exits
/// non-zero if any check failed, which is what CTest keys off.

#include <cstddef>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace testing {

struct TestCase {
  const char *name;
  void (*fn)();
};

/// Thrown by REQUIRE to abandon the current case; caught by the runner so the
/// remaining cases still run.
struct FatalCheck {};

std::vector<TestCase> &registry();

/// Counters the runner snapshots around each case to decide pass/fail.
int failureCount();
int checkCount();

void recordCheck();
void recordFailure(const char *file, int line, const std::string &message);

struct Registrar {
  Registrar(const char *name, void (*fn)()) {
    registry().push_back({name, fn});
  }
};

/// Renders a value for failure output. `uint8_t`-typed fields are everywhere
/// in the wire structs, so integrals are promoted with unary `+` to avoid
/// printing them as unreadable control characters; enums (the sim's `Phase`)
/// print as their underlying number.
template <typename T> std::string show(const T &value) {
  std::ostringstream out;

  if constexpr (std::is_same_v<T, bool>) {
    out << (value ? "true" : "false");
  } else if constexpr (std::is_enum_v<T>) {
    out << static_cast<long long>(value);
  } else if constexpr (std::is_integral_v<T>) {
    out << +value;
  } else {
    out << value;
  }

  return out.str();
}

} // namespace testing

#define TEST(name)                                                             \
  static void name();                                                          \
  static const ::testing::Registrar test_registrar_##name{#name, &name};       \
  static void name()

#define CHECK(expr)                                                            \
  do {                                                                         \
    ::testing::recordCheck();                                                  \
    if (!(expr))                                                               \
      ::testing::recordFailure(__FILE__, __LINE__, "CHECK(" #expr ")");        \
  } while (false)

#define REQUIRE(expr)                                                          \
  do {                                                                         \
    ::testing::recordCheck();                                                  \
    if (!(expr)) {                                                             \
      ::testing::recordFailure(__FILE__, __LINE__, "REQUIRE(" #expr ")");      \
      throw ::testing::FatalCheck{};                                           \
    }                                                                          \
  } while (false)

#define CHECK_EQ(actual, expected)                                             \
  do {                                                                         \
    ::testing::recordCheck();                                                  \
    const auto &actual_ = (actual);                                            \
    const auto &expected_ = (expected);                                        \
    if (!(actual_ == expected_))                                               \
      ::testing::recordFailure(__FILE__, __LINE__,                             \
                               "CHECK_EQ(" #actual ", " #expected ") -> " +    \
                                   ::testing::show(actual_) + " != " +         \
                                   ::testing::show(expected_));                \
  } while (false)

/// Float comparison with an absolute tolerance - positions are the result of
/// repeated `+= v * dt`, so exact equality is the wrong test.
#define CHECK_NEAR(actual, expected, tolerance)                                \
  do {                                                                         \
    ::testing::recordCheck();                                                  \
    const double actual_ = (actual);                                           \
    const double expected_ = (expected);                                       \
    const double diff_ = actual_ > expected_ ? actual_ - expected_             \
                                             : expected_ - actual_;            \
    if (!(diff_ <= (tolerance)))                                               \
      ::testing::recordFailure(__FILE__, __LINE__,                             \
                               "CHECK_NEAR(" #actual ", " #expected ") -> " +  \
                                   ::testing::show(actual_) + " vs " +         \
                                   ::testing::show(expected_));                \
  } while (false)

#define CHECK_THROWS(expr)                                                     \
  do {                                                                         \
    ::testing::recordCheck();                                                  \
    bool threw_ = false;                                                       \
    try {                                                                      \
      (void)(expr);                                                            \
    } catch (...) {                                                            \
      threw_ = true;                                                           \
    }                                                                          \
    if (!threw_)                                                               \
      ::testing::recordFailure(__FILE__, __LINE__,                             \
                               "CHECK_THROWS(" #expr ") did not throw");       \
  } while (false)
