// A tiny catalog with two classes that share a member name, for the
// ambiguity cases the real content cannot show yet (PLAN §12.1, M2).
#pragma once

#include "model/lookup.hpp"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <vector>

namespace fixture {

using namespace cxxman;

inline const Signature string_sig[] = {{"basic_string<CharT>", "", Std::Unknown}};
inline const Signature vector_sig[] = {{"vector<T>", "", Std::Unknown}};
inline const Signature size_sig[] = {{"size() -> size_type", "", Std::Unknown}};

// Sorted by id, like the generated table.
inline const Entry entries[] = {
    {.id = "string", .name = "std::string", .header = "string", .summary = "A string.",
     .url = "string/basic_string", .kind = Kind::Class, .since = Std::Cpp98, .signatures = string_sig},
    {.id = "string::size", .name = "std::string::size", .header = "string",
     .summary = "Number of characters.", .url = "string/basic_string/size", .parent = "string",
     .kind = Kind::Member, .since = Std::Cpp98, .signatures = size_sig},
    {.id = "vector", .name = "std::vector", .header = "vector", .summary = "A vector.",
     .url = "container/vector", .kind = Kind::Class, .since = Std::Cpp98, .signatures = vector_sig},
    {.id = "vector::size", .name = "std::vector::size", .header = "vector",
     .summary = "Number of elements.", .url = "container/vector/size", .parent = "vector",
     .kind = Kind::Member, .since = Std::Cpp98, .signatures = size_sig},
};

inline const IndexName index[] = {
    {"string", "string", "string/basic_string", Kind::Class, Std::Cpp98, 0},
    {"vector", "vector", "container/vector", Kind::Class, Std::Cpp98, 2},
};

// All names are lowercase, so each key is its own folded form.
inline Catalog catalog() {
  static const std::vector<Key> keys = [] {
    std::vector<Key> k;
    for (std::int32_t i = 0; i < 4; ++i) k.push_back({entries[i].id, entries[i].id, Target::Entry, i});
    for (std::int32_t i = 0; i < 2; ++i) k.push_back({index[i].name, index[i].name, Target::Index, i});
    std::ranges::sort(k, [](const Key& a, const Key& b) {
      return std::tie(a.folded, a.key, a.target, a.index) < std::tie(b.folded, b.key, b.target, b.index);
    });
    return k;
  }();
  return {entries, index, keys};
}

}  // namespace fixture
