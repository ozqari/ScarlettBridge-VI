#include "telemetry/refusal_census.h"

#include <cstdio>

namespace sb {
namespace diag {

const char* nullCauseName(NullCause c) {
  switch (c) {
    case NullCause::Entry: return "entry";
    case NullCause::Buffer: return "buffer";
    case NullCause::Token: return "token";
    case NullCause::Wrapper: return "wrapper";
    case NullCause::Dimension: return "dimension";
    case NullCause::None: return "none";
  }
  return "?";
}

NullCause classifyNullBinding(const BindingFacts& f) {
  // A non-texture range that ended up bound is not a null binding at all
  // (the identity fallback still gave the draw something to read).
  if (!f.isTexture && f.rangeBound) return NullCause::None;
  if (f.fromTable && !f.entryReadable) return NullCause::Entry;
  if (f.fromTable && f.tokenMagic && !f.tokenKnown) return NullCause::Token;
  if (f.isTexture) {
    if (!f.tokenMagic) return NullCause::Token;
    if (!f.resourceIsWrapper) return NullCause::Wrapper;
    return f.dimensionMatches ? NullCause::None : NullCause::Dimension;
  }
  return f.rangeBound ? NullCause::None : NullCause::Buffer;
}

void NullCensus::note(NullCause c) {
  int i = static_cast<int>(c);
  if (i < 0 || i >= static_cast<int>(kNullCauses)) return;
  ++counts[static_cast<unsigned>(i)];
}

std::uint64_t NullCensus::total() const {
  std::uint64_t n = 0;
  for (unsigned i = 0; i < kNullCauses; ++i) n += counts[i];
  return n;
}

PerReasonCensus::PerReasonCensus(const std::string& label) {
  label_ = label.empty() ? "per-reason" : label.substr(0, kMaxLabel - 1);
  for (char& c : label_) {
    if (c == '\r' || c == '\n') c = ' ';
  }
}

std::string PerReasonCensus::sanitizeReason(const std::string& s) {
  if (s.empty()) return "unknown";
  std::string o;
  o.reserve(s.size() < kMaxReason - 1 ? s.size() : kMaxReason - 1);
  for (char c : s) {
    if (o.size() + 1 >= kMaxReason) break;
    o += (c == '\r' || c == '\n') ? ' ' : c;
  }
  return o.empty() ? "unknown" : o;
}

bool PerReasonCensus::note(std::uint64_t key, const std::string& reason) {
  std::string r = sanitizeReason(reason);
  std::lock_guard<std::mutex> lock(mu_);
  ++total_;
  for (auto& e : entries_) {
    if (e.key == key && e.reason == r) {
      ++e.calls;
      return false;
    }
  }
  if (entries_.size() >= kMaxEntries) {
    ++overflow_;
    return false;
  }
  if (!hasKeyLocked(key)) ++distinctKeys_;
  bool detail = entries_.size() < kDetailBudget;
  entries_.push_back(Entry{key, r, 1, 0});
  return detail;
}

std::uint64_t PerReasonCensus::total() const {
  std::lock_guard<std::mutex> lock(mu_);
  return total_;
}

unsigned PerReasonCensus::entries() const {
  std::lock_guard<std::mutex> lock(mu_);
  return static_cast<unsigned>(entries_.size());
}

unsigned PerReasonCensus::keys() const {
  std::lock_guard<std::mutex> lock(mu_);
  return distinctKeys_;
}

std::uint64_t PerReasonCensus::overflow() const {
  std::lock_guard<std::mutex> lock(mu_);
  return overflow_;
}

std::vector<std::string> PerReasonCensus::report(bool onlyIfChanged) {
  std::lock_guard<std::mutex> lock(mu_);
  bool changed = (total_ != reported_);
  if (onlyIfChanged) reported_ = total_;
  if (onlyIfChanged && !changed) return {};
  std::vector<std::string> out;
  char line[256];
  std::snprintf(line, sizeof(line),
                "%s refusals: total %llu; distinct keys %u, distinct pairs %u (overflow %llu)",
                label_.c_str(), static_cast<unsigned long long>(total_), distinctKeys_,
                static_cast<unsigned>(entries_.size()),
                static_cast<unsigned long long>(overflow_));
  out.emplace_back(line);
  for (unsigned i = 0; i < entries_.size(); ++i) {
    Entry& e = entries_[i];
    if (onlyIfChanged && e.calls == e.reported) continue;
    if (onlyIfChanged) e.reported = e.calls;
    std::snprintf(line, sizeof(line), "%s refusal %u: key=%016llx reason=%s calls=%llu",
                  label_.c_str(), i + 1, static_cast<unsigned long long>(e.key),
                  e.reason.c_str(), static_cast<unsigned long long>(e.calls));
    out.emplace_back(line);
  }
  return out;
}

bool PerReasonCensus::hasKeyLocked(std::uint64_t key) const {
  for (const auto& e : entries_)
    if (e.key == key) return true;
  return false;
}

}  // namespace diag
}  // namespace sb
