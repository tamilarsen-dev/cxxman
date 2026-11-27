#include "render/json.hpp"

#include "render/card.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace cxxman::render::json {

namespace {

// Writes one document, two spaces per level. Commas and line breaks come
// from the position: the first item of an object or array has no comma.
class Writer {
 public:
  Writer& open(char c) {
    item();
    out_ += c;
    ++depth_;
    first_ = true;
    return *this;
  }
  Writer& close(char c) {
    --depth_;
    if (!first_) newline();
    out_ += c;
    first_ = false;
    return *this;
  }
  Writer& key(std::string_view k) {
    item();
    out_ += quote(k);
    out_ += ": ";
    after_key_ = true;
    return *this;
  }
  Writer& str(std::string_view v) {
    item();
    out_ += quote(v);
    return *this;
  }
  // An empty string is "not known": null.
  Writer& opt(std::string_view v) { return v.empty() ? null() : str(v); }
  Writer& null() {
    item();
    out_ += "null";
    return *this;
  }
  Writer& boolean(bool b) {
    item();
    out_ += b ? "true" : "false";
    return *this;
  }
  Writer& number(std::size_t n) {
    item();
    out_ += std::to_string(n);
    return *this;
  }
  Writer& std_value(Std s) { return opt(to_string(s)); }
  Writer& strings(std::span<const std::string_view> list) {
    open('[');
    for (auto s : list) str(s);
    return close(']');
  }
  // `{"schema": 1, "type": <type>, ...`: the caller closes it with done().
  Writer& document(std::string_view type) {
    open('{');
    key("schema").number(static_cast<std::size_t>(schema));
    return key("type").str(type);
  }
  std::string done() {
    close('}');
    out_ += '\n';
    return std::move(out_);
  }

 private:
  void item() {
    if (after_key_) {
      after_key_ = false;
      return;
    }
    if (!first_) out_ += ',';
    if (depth_ > 0) newline();
    first_ = false;
  }
  void newline() {
    out_ += '\n';
    out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
  }

  std::string out_;
  int depth_ = 0;
  bool first_ = true, after_key_ = false;
};

// Length of the valid UTF-8 sequence at s[i], or 0.
std::size_t utf8_length(std::string_view s, std::size_t i) {
  const auto b = static_cast<unsigned char>(s[i]);
  std::size_t n = 0;
  unsigned char lo = 0x80, hi = 0xBF;  // allowed range of the second byte
  if (b < 0x80) return 1;
  if (b >= 0xC2 && b <= 0xDF) {
    n = 2;
  } else if (b >= 0xE0 && b <= 0xEF) {  // no overlong forms, no surrogates
    n = 3;
    lo = b == 0xE0 ? 0xA0 : 0x80;
    hi = b == 0xED ? 0x9F : 0xBF;
  } else if (b >= 0xF0 && b <= 0xF4) {  // no overlong forms, nothing past U+10FFFF
    n = 4;
    lo = b == 0xF0 ? 0x90 : 0x80;
    hi = b == 0xF4 ? 0x8F : 0xBF;
  } else {
    return 0;
  }
  if (i + n > s.size()) return 0;
  for (std::size_t k = 1; k < n; ++k) {
    const auto c = static_cast<unsigned char>(s[i + k]);
    if (c < (k == 1 ? lo : 0x80) || c > (k == 1 ? hi : 0xBF)) return 0;
  }
  return n;
}

void support(Writer& w, const Support& s) {
  if (s.gcc.empty() && s.clang.empty() && s.apple_clang.empty() && s.msvc.empty() && s.ftm.empty() &&
      s.ftm_value.empty()) {
    w.null();
    return;
  }
  w.open('{');
  w.key("gcc").opt(s.gcc);
  w.key("clang").opt(s.clang);
  w.key("apple_clang").opt(s.apple_clang);
  w.key("msvc").opt(s.msvc);
  w.key("ftm").opt(s.ftm);
  w.key("ftm_value").opt(s.ftm_value);
  w.close('}');
}

void groups(Writer& w, std::span<const Group> gs) {
  w.open('[');
  for (const auto& g : gs) {
    w.open('{');
    w.key("title").str(g.title);
    w.key("names").strings(g.names);
    w.key("since").std_value(g.since);
    w.close('}');
  }
  w.close(']');
}

}  // namespace

std::string quote(std::string_view s) {
  constexpr char hex[] = "0123456789abcdef";
  std::string out = "\"";
  for (std::size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out += hex[c >> 4];
          out += hex[c & 0xF];
        } else if (const std::size_t n = utf8_length(s, i); n == 0) {
          out += "\xEF\xBF\xBD";  // U+FFFD
        } else {
          out.append(s.substr(i, n));
          i += n;
          continue;
        }
    }
    ++i;
  }
  out += '"';
  return out;
}

std::string card(const Entry& e, std::span<const std::string_view> also) {
  Writer w;
  w.document("card");
  w.key("id").str(e.id);
  w.key("name").str(e.name);
  w.key("kind").opt(to_string(e.kind));
  w.key("header").str(e.header);
  w.key("parent").opt(e.parent);
  w.key("since").std_value(e.since);
  w.key("badges").strings(e.badges);
  w.key("summary").str(e.summary);
  w.key("aliases").strings(e.aliases);
  w.key("signatures").open('[');
  for (const auto& s : e.signatures) {
    w.open('{');
    w.key("text").str(s.text);
    w.key("note").opt(s.note);
    w.key("since").std_value(s.since);
    w.close('}');
  }
  w.close(']');
  w.key("members");
  groups(w, e.members);
  w.key("examples").open('[');
  for (const auto& x : e.examples) {
    w.open('{');
    w.key("code").str(x.code);
    w.key("comment").opt(x.comment);
    w.key("since").std_value(x.std);
    w.close('}');
  }
  w.close(']');
  w.key("gotchas").open('[');
  for (const auto& g : e.gotchas) {
    w.open('{');
    w.key("level").str(to_string(g.level));
    w.key("text").str(g.text);
    w.close('}');
  }
  w.close(']');
  w.key("complexity").opt(e.complexity);
  w.key("support");
  support(w, e.support);
  w.key("see_also").strings(e.see_also);
  w.key("url").str(reference_url(e.url));
  w.key("also").strings(also);
  return w.done();
}

std::string index_only(const Catalog& cat, std::span<const std::int32_t> rows,
                       std::span<const std::string_view> also) {
  Writer w;
  w.document("index");
  w.key("name").str(rows.empty() ? std::string_view{} : cat.index[static_cast<std::size_t>(rows[0])].name);
  w.key("places").open('[');
  for (auto r : rows) {
    const IndexName& n = cat.index[static_cast<std::size_t>(r)];
    w.open('{');
    w.key("header").str(n.header);
    w.key("kind").opt(to_string(n.kind));
    w.key("since").std_value(n.since);
    w.key("url").str(reference_url(n.url));
    w.close('}');
  }
  w.close(']');
  w.key("also").strings(also);
  return w.done();
}

std::string ambiguous(std::string_view name, std::span<const std::string_view> candidates) {
  Writer w;
  w.document("ambiguous");
  w.key("name").str(name);
  w.key("candidates").strings(candidates);
  return w.done();
}

std::string missing(std::string_view name, std::span<const std::string_view> suggestions) {
  Writer w;
  w.document("missing");
  w.key("name").str(name);
  w.key("suggestions").strings(suggestions);
  return w.done();
}

std::string search(const Catalog& cat, std::string_view query, std::span<const search::Hit> hits) {
  Writer w;
  w.document("search");
  w.key("query").str(query);
  w.key("results").open('[');
  for (const auto& h : hits) {
    const auto i = static_cast<std::size_t>(h.index);
    std::string_view target, header, summary;
    switch (h.target) {
      case Target::Entry:
        target = "card";
        header = cat.entries[i].header;
        summary = cat.entries[i].summary;
        break;
      case Target::Recipe:
        target = "recipe";
        summary = cat.recipes[i].summary;
        break;
      case Target::Compare:
        target = "compare";
        summary = cat.compares[i].title;
        break;
      case Target::Index:
        target = "index";
        header = cat.index[i].header;
        break;
    }
    w.open('{');
    w.key("target").str(target);
    w.key("name").str(cxxman::search::hit_name(cat, h));
    w.key("header").opt(header);
    w.key("summary").opt(summary);
    w.key("headers").number(h.headers);
    w.close('}');
  }
  w.close(']');
  return w.done();
}

std::string header(std::string_view name, const Header* h, std::span<const NameInfo> names) {
  Writer w;
  w.document("header");
  w.key("header").str(name);
  w.key("summary").opt(h ? h->summary : std::string_view{});
  w.key("groups");
  groups(w, h ? h->groups : std::span<const Group>{});
  w.key("names").open('[');
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i > 0 && names[i].name == names[i - 1].name) continue;
    w.open('{');
    w.key("name").str(names[i].name);
    w.key("card").boolean(names[i].entry >= 0);
    w.key("kind").opt(to_string(names[i].kind));
    w.key("since").std_value(names[i].since);
    w.close('}');
  }
  w.close(']');
  return w.done();
}

}  // namespace cxxman::render::json
