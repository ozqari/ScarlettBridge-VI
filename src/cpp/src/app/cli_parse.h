#pragma once
// Strict option parser for CLI subcommands (`scarlettbridge-cli ...`,
// `scarlettbridge-runtime ...`).
//
// Idea informed by XCL `crates/xcl/src/cli.rs`: reject unknown options,
// stray positional arguments, missing option values, and malformed
// numbers/hashes instead of silently falling back to defaults — plus the
// same strictness Xbox2Windows `export_write_mask.h` applies to its manifest
// lines (exact shape, no second line for the same target). Independently
// implemented here in portable C++ with our own types and wording; no OS
// calls, no game content. Unit-tested in sb_tests.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace cli {

// Parses one subcommand's trailing arguments left to right.
class Parser {
 public:
  explicit Parser(const std::vector<std::string>& args);

  // Next raw argument (option or positional), or nullopt at the end.
  std::optional<std::string> nextArg();
  bool atEnd() const;

  // Required value following option `name`. Fails on end-of-arguments or
  // when another `--...` flag sits in its place ("{name} needs a value").
  Result<std::string> value(const std::string& name);
  // value() parsed as a decimal integer ("invalid value for {name}: {v}").
  Result<std::uint64_t> parseU64(const std::string& name);
  // value() parsed as "0"/"1" ("... (expected 0 or 1)").
  Result<bool> parseBool01(const std::string& name);
  // value() parsed as hex ("{name}: ...").
  Result<std::uint64_t> parseHexU64(const std::string& name);

  // "unknown option: {arg}" for `--...`, "unexpected argument: {arg}".
  static std::string unexpected(const std::string& arg);
  // Bare or 0x-prefixed hex, 1-16 digits; anything else fails.
  static Result<std::uint64_t> parseHexU64Str(const std::string& s);

 private:
  const std::vector<std::string>& args_;
  std::size_t index_ = 0;
};

}  // namespace cli
}  // namespace sb
