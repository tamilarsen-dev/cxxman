// The interactive finder (PLAN §4.6): a query line, the ranked list and a
// preview. The state and the frames it draws are apart from any terminal, so
// the tests drive it with keys and read its rows.
#pragma once

#include "model/lookup.hpp"
#include "render/card.hpp"
#include "render/style.hpp"
#include "term/keys.hpp"
#include "term/tty.hpp"
#include "tui/finder_model.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cxxman::tui {

struct FinderSettings {
  render::Style style;       // colors of the rows
  bool ascii = false;        // no Unicode symbols or box drawing
  Std std = Std::Unknown;    // --std for the preview
  bool strict = false;       // --strict for the preview
};

class Finder {
 public:
  // What a key asks for beyond moving and typing; the caller carries it out.
  enum class Action : std::uint8_t { None, Quit, Open, Print, Copy, Web };

  // `cat` and `model` must outlive the finder.
  Finder(const Catalog& cat, const FinderModel& model, FinderSettings s);

  // Handles one key on a screen of size `s`.
  Action handle(const term::Key& k, term::Size s);

  // One painted string per screen row, top first.
  std::vector<std::string> frame(term::Size s);

  // The column (from 1) of the cursor on the query line.
  std::size_t cursor_column() const;

  const std::string& query() const { return query_; }
  std::size_t match_count() const { return matches_.size(); }
  // The selected item, or null when nothing matches.
  const Item* selected() const;
  // Whether the preview shows only the examples (Ctrl-E).
  bool examples() const { return examples_; }
  Std standard() const { return settings_.std; }

 private:
  enum class Mode : std::uint8_t { Side, Stacked, List };
  struct Layout {
    Mode mode = Mode::List;
    std::size_t list_rows = 0, list_w = 0, preview_rows = 0, preview_w = 0;
  };
  Layout layout(term::Size s) const;
  void requery();
  void move(std::ptrdiff_t by, std::size_t rows);
  render::Line prompt(std::size_t cols) const;
  render::Line help(std::size_t cols) const;
  std::vector<render::Line> list(std::size_t rows, std::size_t width) const;
  const std::vector<render::Line>& preview(std::size_t width);
  std::string paint(const render::Line& line) const;

  const Catalog* cat_;
  const FinderModel* model_;
  FinderSettings settings_;
  std::vector<NameInfo> known_;                       // for header previews
  std::map<std::string_view, std::size_t> per_header_;  // names per header
  std::size_t header_count_ = 0;

  std::string query_;
  std::vector<Match> matches_;
  std::size_t sel_ = 0, top_ = 0;
  std::optional<bool> preview_on_;  // unset: on when the screen is 60+ wide
  bool examples_ = false;

  // The preview of one item at one width, kept until either changes.
  struct Cached {
    std::uint32_t item = 0;
    std::size_t width = 0;
    Std std = Std::Unknown;
    bool examples = false;
    bool valid = false;
  } cached_;
  std::vector<render::Line> preview_lines_;
};

// Shows the finder on `tty` until a key asks for an action (Open, Print,
// Copy, Web, Quit) or the input goes away (Quit). The query and selection
// stay in `f`, so calling it again returns to the same place.
Finder::Action run(term::Tty& tty, Finder& f);

}  // namespace cxxman::tui
