#pragma once
// Минимальный тестовый каркас (без внешних зависимостей): TEST, CHECK, CHECK_EQ, CHECK_THROWS.
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct Case {
  std::string suite, name;
  std::function<void()> fn;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}
inline int& failures() {
  static int n = 0;
  return n;
}
struct Registrar {
  Registrar(const char* suite, const char* name, std::function<void()> fn) {
    registry().push_back({suite, name, std::move(fn)});
  }
};
inline void fail(const char* file, int line, const std::string& msg) {
  ++failures();
  std::cerr << file << ":" << line << ": " << msg << "\n";
}
template <class T>
std::string str(const T& v) {
  std::ostringstream o;
  o << v;
  return o.str();
}

}  // namespace testing

#define TEST(suite, name)                                                                   \
  static void suite##_##name();                                                             \
  static testing::Registrar reg_##suite##_##name(#suite, #name, suite##_##name);            \
  static void suite##_##name()

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) testing::fail(__FILE__, __LINE__, "CHECK failed: " #cond);  \
  } while (0)

#define CHECK_EQ(a, b)                                                                                     \
  do {                                                                                                     \
    const auto _l = (a);                                                                                   \
    const auto _r = (b);                                                                                   \
    if (!(_l == _r))                                                                                       \
      testing::fail(__FILE__, __LINE__,                                                                    \
                    std::string("CHECK_EQ failed: " #a " == " #b "\n   left:  ") + testing::str(_l) +      \
                        "\n   right: " + testing::str(_r));                                                \
  } while (0)

#define CHECK_THROWS(expr, ExType)                                                              \
  do {                                                                                          \
    bool _thrown = false;                                                                       \
    try {                                                                                       \
      (void)(expr);                                                                             \
    } catch (const ExType&) {                                                                   \
      _thrown = true;                                                                           \
    } catch (...) {                                                                             \
    }                                                                                           \
    if (!_thrown) testing::fail(__FILE__, __LINE__, "expected " #ExType " from: " #expr);       \
  } while (0)
