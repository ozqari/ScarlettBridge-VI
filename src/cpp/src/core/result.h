#pragma once
// Explicit error/result plumbing. No exceptions cross module boundaries.
#include <string>
#include <utility>

namespace sb {

struct Error {
  std::string category;  // e.g. "config", "ipc", "graphics/d3d12"
  int code = 0;          // module-local code; 0 = unspecified
  std::string message;
  static Error make(std::string cat, int c, std::string msg) {
    return Error{std::move(cat), c, std::move(msg)};
  }
};

template <typename T>
class Result {
 public:
  static Result ok(T v) { return Result(true, std::move(v), Error{}); }
  static Result fail(Error e) { return Result(false, T{}, std::move(e)); }
  bool ok() const { return ok_; }
  const T& value() const { return value_; }
  T& value() { return value_; }
  const Error& error() const { return err_; }
  template <typename F>
  auto map(F&& f) -> Result<decltype(f(value_))> {
    using U = decltype(f(value_));
    if (!ok_) return Result<U>::fail(err_);
    return Result<U>::ok(f(value_));
  }
 private:
  Result(bool o, T v, Error e) : ok_(o), value_(std::move(v)), err_(std::move(e)) {}
  bool ok_;
  T value_;
  Error err_;
};

template <>
class Result<void> {
 public:
  static Result ok() { return Result(true, Error{}); }
  static Result fail(Error e) { return Result(false, std::move(e)); }
  bool ok() const { return ok_; }
  const Error& error() const { return err_; }
 private:
  Result(bool o, Error e) : ok_(o), err_(std::move(e)) {}
  bool ok_;
  Error err_;
};

}  // namespace sb
