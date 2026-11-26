#pragma once
// Minimal strict JSON value + parser/serializer for settings and IPC.
// Supported: null, bool, number (double), string, array, object.
// Strict: no comments/trailing commas, depth cap, size caps, no trailing garbage.
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "core/result.h"

namespace sb {
namespace json {

class Value {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };
  Value() : type_(Type::Null) {}
  static Value null() { return Value(); }
  static Value boolean(bool b);
  static Value number(double d);
  static Value str(std::string s);
  static Value array();
  static Value object();
  Type type() const { return type_; }
  bool isNull() const { return type_ == Type::Null; }
  bool asBool(bool fallback = false) const;
  double asNumber(double fallback = 0.0) const;
  const std::string& asString() const;
  const std::vector<Value>& asArray() const;
  const std::map<std::string, Value>& asObject() const;
  std::vector<Value>& asArray();
  std::map<std::string, Value>& asObject();
  void push(Value v);
  void set(const std::string& key, Value v);
  bool has(const std::string& key) const;
  const Value& at(const std::string& key) const;
  std::string serialize() const;
  static Result<Value> parse(const std::string& text);

 private:
  explicit Value(Type t) : type_(t) {}
  Type type_ = Type::Null;
  bool boolean_ = false;
  double number_ = 0.0;
  std::string string_;
  std::vector<Value> array_;
  std::map<std::string, Value> object_;
};

}  // namespace json
}  // namespace sb
