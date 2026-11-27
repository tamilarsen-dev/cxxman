// --json escaping and the header document (docs/JSON.md, PLAN §10).
#include "render/json.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using namespace cxxman;
namespace json = cxxman::render::json;

TEST_CASE("json quote escapes what JSON needs and keeps UTF-8") {
  CHECK(json::quote("sort") == "\"sort\"");
  CHECK(json::quote("a \"b\" c") == "\"a \\\"b\\\" c\"");
  CHECK(json::quote("C:\\dir") == "\"C:\\\\dir\"");
  CHECK(json::quote("a\nb\tc\r") == "\"a\\nb\\tc\\r\"");
  CHECK(json::quote("\b\f") == "\"\\b\\f\"");
  CHECK(json::quote(std::string_view("\x01\x1f", 2)) == "\"\\u0001\\u001f\"");
  CHECK(json::quote(std::string_view("a\0b", 3)) == "\"a\\u0000b\"");
  CHECK(json::quote("/ < > '") == "\"/ < > '\"");     // nothing else is escaped
  CHECK(json::quote("✗ · … 中文") == "\"✗ · … 中文\"");  // UTF-8 passes through
  CHECK(json::quote("\xF0\x9F\x98\x80") == "\"\xF0\x9F\x98\x80\"");  // 4-byte sequence
}

TEST_CASE("json quote replaces bytes that are not UTF-8 with U+FFFD") {
  const std::string bad = "\"\xEF\xBF\xBD\"";
  CHECK(json::quote("\xff") == bad);
  CHECK(json::quote("\x80") == bad);              // a lone continuation byte
  CHECK(json::quote("\xC0\xAF") == "\"\xEF\xBF\xBD\xEF\xBF\xBD\"");  // overlong '/'
  CHECK(json::quote("\xED\xA0\x80").starts_with("\"\xEF\xBF\xBD"));  // a surrogate
  CHECK(json::quote("\xE2\x9C") == "\"\xEF\xBF\xBD\xEF\xBF\xBD\"");  // cut short
  CHECK(json::quote("a\xffz") == "\"a\xEF\xBF\xBDz\"");
}

TEST_CASE("json header: groups from the header file, every name once") {
  static constexpr std::string_view sort_names[] = {"sort", "stable_sort"};
  static const Group groups[] = {{"Sort", sort_names, Std::Unknown}};
  const Header h{"algorithm", "Algorithms.", groups};
  const NameInfo names[] = {
      {"sort", "algorithm", Kind::Algorithm, Std::Cpp98, 0},
      {"stable_sort", "algorithm", Kind::Unknown, Std::Unknown, -1},
      {"stable_sort", "algorithm", Kind::Unknown, Std::Unknown, -1},  // same name twice
  };
  const std::string out = json::header("algorithm", &h, names);
  CHECK(out ==
        "{\n"
        "  \"schema\": 1,\n"
        "  \"type\": \"header\",\n"
        "  \"header\": \"algorithm\",\n"
        "  \"summary\": \"Algorithms.\",\n"
        "  \"groups\": [\n"
        "    {\n"
        "      \"title\": \"Sort\",\n"
        "      \"names\": [\n"
        "        \"sort\",\n"
        "        \"stable_sort\"\n"
        "      ],\n"
        "      \"since\": null\n"
        "    }\n"
        "  ],\n"
        "  \"names\": [\n"
        "    {\n"
        "      \"name\": \"sort\",\n"
        "      \"card\": true,\n"
        "      \"kind\": \"algorithm\",\n"
        "      \"since\": \"C++98\"\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"stable_sort\",\n"
        "      \"card\": false,\n"
        "      \"kind\": null,\n"
        "      \"since\": null\n"
        "    }\n"
        "  ]\n"
        "}\n");
  CHECK(json::header("x", nullptr, {}).find("\"groups\": [],") != std::string::npos);
}
