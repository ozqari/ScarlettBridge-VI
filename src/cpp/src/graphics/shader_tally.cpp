#include "graphics/shader_tally.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace sb {
namespace gfx {

FailTally::FailTally(bool list) : list_(list) {}

bool FailTally::list() const { return list_; }

void FailTally::fail(const std::string& reason, const std::string& example) {
  try {
    if (list_) lines_.push_back(example + "  " + reason);
    auto it = failed_.find(reason);
    if (it == failed_.end()) {
      failed_.emplace(reason, std::make_pair(std::size_t{1}, example));
    } else {
      it->second.first += 1;
    }
    total_ += 1;
  } catch (...) {
    // Never throws: on allocation failure the count is left as it was.
  }
}

void FailTally::merge(const FailTally& other) {
  try {
    lines_.insert(lines_.end(), other.lines_.begin(), other.lines_.end());
    for (const auto& kv : other.failed_) {
      auto it = failed_.find(kv.first);
      if (it == failed_.end()) {
        failed_.emplace(kv.first, kv.second);
      } else {
        it->second.first += kv.second.first;  // our first example wins
      }
    }
    total_ += other.total_;
  } catch (...) {
    // Never throws: partial folds keep what fit.
  }
}

std::size_t FailTally::total() const { return total_; }

std::size_t FailTally::reasons() const { return failed_.size(); }

std::pair<std::size_t, std::string> FailTally::countFor(
    const std::string& reason) const {
  try {
    auto it = failed_.find(reason);
    if (it != failed_.end()) return it->second;
  } catch (...) {
  }
  return {std::size_t{0}, std::string()};
}

std::vector<std::string> FailTally::listLines() const {
  try {
    std::vector<std::string> out = lines_;
    std::sort(out.begin(), out.end());
    return out;
  } catch (...) {
    return {};
  }
}

std::vector<std::string> FailTally::report() const {
  try {
    // failed_ iterates reason-ascending (std::map); the stable sort keeps
    // tied counts in that order, exactly like the upstream stable sort
    // over its BTreeMap.
    std::vector<std::pair<std::string, std::pair<std::size_t, std::string>>> rows(
        failed_.begin(), failed_.end());
    std::stable_sort(rows.begin(), rows.end(),
                     [](const auto& a, const auto& b) {
                       return a.second.first > b.second.first;
                     });
    std::vector<std::string> out;
    for (const auto& kv : rows) {
      std::ostringstream o;
      o << "  " << std::setw(7) << kv.second.first << "  " << kv.first
        << "  (e.g. " << kv.second.second << ")";
      out.push_back(o.str());
    }
    return out;
  } catch (...) {
    return {};
  }
}

}  // namespace gfx
}  // namespace sb
