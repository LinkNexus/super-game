#include "test_harness.h"

#include <cstdio>
#include <exception>

namespace testing {
namespace {
int g_failures = 0;
int g_checks = 0;
} // namespace

std::vector<TestCase> &registry() {
  // Function-local static so registration from other translation units'
  // static initializers can't race the vector's own construction.
  static std::vector<TestCase> tests;
  return tests;
}

int failureCount() { return g_failures; }
int checkCount() { return g_checks; }

void recordCheck() { ++g_checks; }

void recordFailure(const char *file, int line, const std::string &message) {
  ++g_failures;
  std::printf("         %s:%d\n           %s\n", file, line, message.c_str());
}

} // namespace testing

int main() {
  const auto &tests = testing::registry();
  int failedTests = 0;

  for (const auto &test : tests) {
    const int failuresBefore = testing::failureCount();

    try {
      test.fn();
    } catch (const testing::FatalCheck &) {
      // REQUIRE already reported the reason.
    } catch (const std::exception &e) {
      testing::recordFailure(test.name, 0,
                             std::string("unexpected exception: ") + e.what());
    } catch (...) {
      testing::recordFailure(test.name, 0, "unexpected non-standard exception");
    }

    const bool passed = testing::failureCount() == failuresBefore;
    if (!passed)
      ++failedTests;

    std::printf("[ %s ] %s\n", passed ? "PASS" : "FAIL", test.name);
  }

  std::printf("\n%zu tests (%d failed), %d checks (%d failed)\n", tests.size(),
              failedTests, testing::checkCount(), testing::failureCount());

  return failedTests == 0 ? 0 : 1;
}
