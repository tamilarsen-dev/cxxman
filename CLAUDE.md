# cxxman — agent rules

cxxman is a fast, curated C++ standard-library reference CLI in C++20:
cards, an interactive finder, recipes, compares, and verified
self-update. The spec is `docs/PLAN.md`; the content rules are in
`docs/STYLE.md`. Read both before your first change. This file wins
over them on conflict. If PLAN and STYLE conflict, ask the owner.

## Workflow

- Work on exactly one milestone from PLAN §12.1 at a time, in order. It
  is done only when its "Done when" criteria pass. Mark it with
  `✅ <date>` in PLAN.
- Start each session with `git status` and PLAN §12.1 to find the first
  unfinished milestone.
- Build: `cmake --preset dev && cmake --build --preset dev`.
  Test: `ctest --preset dev`. Run both before every commit. Configure with
  `-DCXXMAN_CHECK_SNIPPETS=ON` whenever you touch `data/`.
- Make small commits with imperative messages.
- **Never add `Co-Authored-By`, "Generated with", or any Claude/AI
  attribution to commits, tags, or PR text.**
- **Never push.** The owner pushes to GitHub.
- When a decision isn't covered by the plan, make the smallest
  reasonable choice, add a row to PLAN §14, and mention it in the commit.

## Hard rules

- No runtime parsing of card data; it is compiled in. The only runtime
  file read is the optional config (PLAN §7.5).
- Network only in `update` commands (PLAN §9). No telemetry and no
  background checks.
- No dependencies beyond toml++ (generator) and doctest (tests). The
  runtime uses only the C++ standard library and OS facilities.
- Platform code lives only in `src/term/`, `src/sys/`, `src/net/` and
  `src/update/`, inside `.cpp` files.
- The release asset names and archive layout in PLAN §11.3 are a
  contract with installed v0.3.1 clients. Never change them.
- Python is allowed **only** in `tools/upstream/`, stdlib only, and it is
  never shipped.
- Golden diffs must be intentional. Read every diff before regenerating.
- The terminal must be restored on every exit path of the finder and
  pager.

## Content rules (summary of STYLE.md)

- Every snippet compiles in CI on GCC, Clang and MSVC. Fix failing
  snippets; never weaken warnings. `skip` is only for real toolchain
  gaps, with a reason.
- Write in your own words. Never copy cppreference sentences or store
  their prose anywhere in the repo.
- If you cannot verify a fact (complexity, `since`, badge, exceptions,
  `[support]`), omit it and write `unverified: <what>` in the commit
  message.
- Respect the budgets. Cut content; never raise limits.
- To re-verify a stale item from the "Upstream changes" issue: fetch the
  upstream page → re-check every fact → fix the TOML → run the snippet
  tests → `python tools/upstream/accept.py <id>` → commit "Re-verify <id>
  against upstream".

## Legacy

Branch `legacy-python` holds the old v0.3.1. Read it only for M1
(`catalog.json` → `names.tsv`) and M7 (installer `.iss`, updater
contract). Never copy its parser or stored cppreference text.
