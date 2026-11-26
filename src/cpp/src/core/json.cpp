#include "core/json.h"
#include <cctype>
#include <cstdio>
#include <limits>

namespace sb {
namespace json {

Value Value::boolean(bool b) { Value v(Type::Bool); v.boolean_ = b; return v; }
Value Value::number(double d) { Value v(Type::Number); v.number_ = d; return v; }
Value Value::str(std::string s) { Value v(Type::String); v.string_ = std::move(s); return v; }
Value Value::array() { return Value(Type::Array); }
Value Value::object() { return Value(Type::Object); }

bool Value::asBool(bool fallback) const { return type_ == Type::Bool ? boolean_ : fallback; }
double Value::asNumber(double fallback) const { return type_ == Type::Number ? number_ : fallback; }
const std::string& Value::asString() const { return string_; }
const std::vector<Value>& Value::asArray() const { return array_; }
const std::map<std::string, Value>& Value::asObject() const { return object_; }
std::vector<Value>& Value::asArray() { return array_; }
std::map<std::string, Value>& Value::asObject() { return object_; }
void Value::push(Value v) { array_.push_back(std::move(v)); }
void Value::set(const std::string& key, Value v) { object_[key] = std::move(v); }
bool Value::has(const std::string& key) const {
  return type_ == Type::Object && object_.find(key) != object_.end();
}
namespace {
const Value kNull;
}
const Value& Value::at(const std::string& key) const {
  if (type_ != Type::Object) return kNull;
  auto it = object_.find(key);
  return it == object_.end() ? kNull : it->second;
}

namespace {
void escapeInto(const std::string& s, std::string& out) {
  out.push_back('"');
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
}
void serializeInto(const Value& v, std::string& out) {
  using T = Value::Type;
  switch (v.type()) {
    case T::Null: out += "null"; break;
    case T::Bool: out += v.asBool() ? "true" : "false"; break;
    case T::Number: {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.17g", v.asNumber());
      out += buf;
      break;
    }
    case T::String: escapeInto(v.asString(), out); break;
    case T::Array: {
      out.push_back('[');
      bool first = true;
      for (const auto& e : v.asArray()) {
        if (!first) out.push_back(',');
        first = false;
        serializeInto(e, out);
      }
      out.push_back(']');
      break;
    }
    case T::Object: {
      out.push_back('{');
      bool first = true;
      for (const auto& kv : v.asObject()) {
        if (!first) out.push_back(',');
        first = false;
        escapeInto(kv.first, out);
        out.push_back(':');
        serializeInto(kv.second, out);
      }
      out.push_back('}');
      break;
    }
  }
}

class Parser {
 public:
  explicit Parser(const std::string& t) : t_(t) {}
  Result<Value> run() {
    skipWs();
    Result<Value> v = parseValue(0);
    if (!v.ok()) return v;
    skipWs();
    if (pos_ != t_.size()) return Result<Value>::fail(Error::make("json", 1, "trailing characters"));
    return v;
  }
 private:
  static constexpr int kMaxDepth = 64;
  const std::string& t_;
  std::size_t pos_ = 0;
  void skipWs() {
    while (pos_ < t_.size() && (t_[pos_] == ' ' || t_[pos_] == '\t' || t_[pos_] == '\n' || t_[pos_] == '\r')) ++pos_;
  }
  Result<Value> parseValue(int depth) {
    if (depth > kMaxDepth) return Result<Value>::fail(Error::make("json", 2, "nesting too deep"));
    if (pos_ >= t_.size()) return Result<Value>::fail(Error::make("json", 3, "unexpected end"));
    char c = t_[pos_];
    if (c == '{') return parseObject(depth);
    if (c == '[') return parseArray(depth);
    if (c == '"') return parseString().map([](std::string s) { return Value::str(std::move(s)); });
    if (c == 't' || c == 'f' || c == 'n') return parseLiteral();
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
    return Result<Value>::fail(Error::make("json", 4, std::string("unexpected character '") + c + "'"));
  }
  Result<Value> parseObject(int depth) {
    ++pos_;
    Value obj = Value::object();
    skipWs();
    if (pos_ < t_.size() && t_[pos_] == '}') { ++pos_; return Result<Value>::ok(std::move(obj)); }
    while (true) {
      skipWs();
      if (pos_ >= t_.size() || t_[pos_] != '"')
        return Result<Value>::fail(Error::make("json", 5, "object key must be a string"));
      Result<std::string> key = parseString();
      if (!key.ok()) return Result<Value>::fail(key.error());
      skipWs();
      if (pos_ >= t_.size() || t_[pos_] != ':')
        return Result<Value>::fail(Error::make("json", 6, "expected ':'"));
      ++pos_;
      skipWs();
      Result<Value> val = parseValue(depth + 1);
      if (!val.ok()) return val;
      obj.set(key.value(), std::move(val.value()));
      skipWs();
      if (pos_ >= t_.size()) return Result<Value>::fail(Error::make("json", 7, "unterminated object"));
      if (t_[pos_] == '}') { ++pos_; return Result<Value>::ok(std::move(obj)); }
      if (t_[pos_] != ',') return Result<Value>::fail(Error::make("json", 8, "expected ',' or '}'"));
      ++pos_;
    }
  }
  Result<Value> parseArray(int depth) {
    ++pos_;
    Value arr = Value::array();
    skipWs();
    if (pos_ < t_.size() && t_[pos_] == ']') { ++pos_; return Result<Value>::ok(std::move(arr)); }
    while (true) {
      skipWs();
      Result<Value> val = parseValue(depth + 1);
      if (!val.ok()) return val;
      arr.push(std::move(val.value()));
      if (arr.asArray().size() > 100000)
        return Result<Value>::fail(Error::make("json", 9, "array too large"));
      skipWs();
      if (pos_ >= t_.size()) return Result<Value>::fail(Error::make("json", 10, "unterminated array"));
      if (t_[pos_] == ']') { ++pos_; return Result<Value>::ok(std::move(arr)); }
      if (t_[pos_] != ',') return Result<Value>::fail(Error::make("json", 11, "expected ',' or ']'"));
      ++pos_;
    }
  }
  Result<std::string> parseString() {
    ++pos_;  // opening quote
    std::string out;
    while (pos_ < t_.size()) {
      char c = t_[pos_++];
      if (c == '"') {
        if (out.size() > 1024u * 1024u)
          return Result<std::string>::fail(Error::make("json", 12, "string too large"));
        return Result<std::string>::ok(std::move(out));
      }
      if (c == '\\') {
        if (pos_ >= t_.size()) break;
        char e = t_[pos_++];
        switch (e) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'u': {
            if (pos_ + 4 > t_.size()) return Result<std::string>::fail(Error::make("json", 13, "bad \\u escape"));
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
              char h = t_[pos_++];
              code <<= 4;
              if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
              else return Result<std::string>::fail(Error::make("json", 14, "bad \\u escape"));
            }
            // BMP only; encode as UTF-8.
            if (code < 0x80) out.push_back(static_cast<char>(code));
            else if (code < 0x800) {
              out.push_back(static_cast<char>(0xC0 | (code >> 6)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
              out.push_back(static_cast<char>(0xE0 | (code >> 12)));
              out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            break;
          }
          default: return Result<std::string>::fail(Error::make("json", 15, "bad escape"));
        }
      } else {
        out.push_back(c);
      }
    }
    return Result<std::string>::fail(Error::make("json", 16, "unterminated string"));
  }
  Result<Value> parseLiteral() {
    if (t_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Result<Value>::ok(Value::boolean(true)); }
    if (t_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Result<Value>::ok(Value::boolean(false)); }
    if (t_.compare(pos_, 4, "null") == 0) { pos_ += 4; return Result<Value>::ok(Value::null()); }
    return Result<Value>::fail(Error::make("json", 17, "bad literal"));
  }
  Result<Value> parseNumber() {
    std::size_t start = pos_;
    if (t_[pos_] == '-') ++pos_;
    while (pos_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[pos_]))) ++pos_;
    if (pos_ < t_.size() && t_[pos_] == '.') {
      ++pos_;
      while (pos_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[pos_]))) ++pos_;
    }
    if (pos_ < t_.size() && (t_[pos_] == 'e' || t_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < t_.size() && (t_[pos_] == '+' || t_[pos_] == '-')) ++pos_;
      while (pos_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[pos_]))) ++pos_;
    }
    try {
      double d = std::stod(t_.substr(start, pos_ - start));
      return Result<Value>::ok(Value::number(d));
    } catch (...) {
      return Result<Value>::fail(Error::make("json", 18, "bad number"));
    }
  }
};
}  // namespace

std::string Value::serialize() const {
  std::string out;
  serializeInto(*this, out);
  return out;
}

Result<Value> Value::parse(const std::string& text) {
  if (text.size() > 8u * 1024u * 1024u)
    return Result<Value>::fail(Error::make("json", 19, "document too large"));
  Parser p(text);
  return p.run();
}

}  // namespace json
}  // namespace sb
