#include <cstring>

#include "check.h"

int main(int argc, char** argv) {
  const char* suite = argc > 1 ? argv[1] : nullptr;
  int run = 0;
  for (const auto& c : testing::registry()) {
    if (suite && c.suite != suite) continue;
    const int before = testing::failures();
    try {
      c.fn();
    } catch (const std::exception& e) {
      testing::fail(__FILE__, __LINE__, "unexpected exception: " + std::string(e.what()));
    }
    ++run;
    std::cout << (testing::failures() == before ? "[ ok ] " : "[FAIL] ") << c.suite << "." << c.name << "\n";
  }
  std::cout << run << " tests, " << testing::failures() << " failures\n";
  if (run == 0) return 2;  // опечатка в имени набора
  return testing::failures() == 0 ? 0 : 1;
}
