#include "app/cli_parse.h"

namespace sb {
namespace cli {

Parser::Parser(const std::vector<std::string>& args) : args_(args) {}

std::optional<std::string> Parser::nextArg() {
  if (index_ >= args_.size()) return std::nullopt;
  return args_[index_++];
}

bool Parser::atEnd() const { return index_ >= args_.size(); }

Result<std::string> Parser::value(const std::string& name) {
  if (index_ >= args_.size() || args_[index_].rfind("--", 0) == 0)
    return Result<std::string>::fail(Error::make("cli", 1, name + " needs a value"));
  return Result<std::string>::success(args_[index_++]);
}

Result<std::uint64_t> Parser::parseU64(const std::string& name) {
  auto v = value(name);
  if (!v.ok()) return Result<std::uint64_t>::fail(v.error());
  const std::string& s = v.value();
  if (s.empty()) return Result<std::uint64_t>::fail(Error::make("cli", 2, "invalid value for " + name + ": " + s));
  for (char c : s) {
    if (c < '0' || c > '9')
      return Result<std::uint64_t>::fail(Error::make("cli", 2, "invalid value for " + name + ": " + s));
  }
  try {
    return Result<std::uint64_t>::success(std::stoull(s));
  } catch (...) {
    return Result<std::uint64_t>::fail(Error::make("cli", 2, "invalid value for " + name + ": " + s));
  }
}

Result<bool> Parser::parseBool01(const std::string& name) {
  auto v = value(name);
  if (!v.ok()) return Result<bool>::fail(v.error());
  if (v.value() == "0") return Result<bool>::success(false);
  if (v.value() == "1") return Result<bool>::success(true);
  return Result<bool>::fail(
      Error::make("cli", 3, "invalid value for " + name + ": " + v.value() + " (expected 0 or 1)"));
}

Result<std::uint64_t> Parser::parseHexU64(const std::string& name) {
  auto v = value(name);
  if (!v.ok()) return Result<std::uint64_t>::fail(v.error());
  auto h = parseHexU64Str(v.value());
  if (!h.ok()) return Result<std::uint64_t>::fail(Error::make("cli", 4, name + ": " + h.error().message));
  return h;
}

std::string Parser::unexpected(const std::string& arg) {
  if (arg.rfind("--", 0) == 0) return "unknown option: " + arg;
  if (!arg.empty() && arg[0] == '-') return "unknown option: " + arg;
  return "unexpected argument: " + arg;
}

Result<std::uint64_t> Parser::parseHexU64Str(const std::string& s) {
  std::string hex = s;
  if (hex.rfind("0x", 0) == 0 || hex.rfind("0X", 0) == 0) hex = hex.substr(2);
  if (hex.empty() || hex.size() > 16) {
    return Result<std::uint64_t>::fail(
        Error::make("cli", 5, "\"" + s + "\" is not a 1-16 digit hexadecimal value"));
  }
  for (char c : hex) {
    bool digit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!digit) {
      return Result<std::uint64_t>::fail(
          Error::make("cli", 5, "\"" + s + "\" is not a 1-16 digit hexadecimal value"));
    }
  }
  try {
    return Result<std::uint64_t>::success(std::stoull(hex, nullptr, 16));
  } catch (...) {
    return Result<std::uint64_t>::fail(
        Error::make("cli", 5, "\"" + s + "\" is not a 1-16 digit hexadecimal value"));
  }
}

}  // namespace cli
}  // namespace sb
