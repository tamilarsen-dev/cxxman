# Project Handoff

Last updated: 2026-11-22

## Current state

- Public repository: <https://github.com/tamilarsen-dev/cxxman>
- Latest release: [`v0.2.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.2.0)
- The release includes Windows x64, Linux x64, Intel macOS, and Apple
  Silicon macOS archives, plus SHA-256 sidecar files.
- GitHub Actions passed the test, smoke-test, and packaging jobs for all
  four platforms. The tag workflow published the release successfully.
- MIT license is in the root `LICENSE` file.
- Published `v0.2.0` passed its release test, packaging, and installer
  smoke-test workflows. The local suite after M3 passes 75 tests.
- The Windows setup and generated uninstaller passed CI install/remove
  coverage.
- macOS binaries are not signed or notarized. Linux x64 uses Ubuntu 22.04
  as the compatibility baseline.

## Git state at handoff

- Branch: `master`, tracking `origin/master`.
- The release is tagged `v0.2.0` on `master`.
- The original commit author/committer addresses were rewritten to the
  account's GitHub noreply address before the public push. A local backup
  branch preserves the original history; do not push that backup branch.
- The working tree contains uncommitted follow-up implementation, tests,
  fixtures, catalog data, and documentation, including the M1-M8 work
  described below. No commit was requested; preserve these changes.

## Next planned work

M1 real cppreference fixtures, M2 parser fidelity, M3 quick-card
projection/rendering, M4 storage format, M5 CLI UX, M6 catalog/header
index, M7 scale ingestion, and M8 coverage validation are complete. M3
passed 75 tests; the full suite after M4 passed 78 tests; M5 passed 88
tests; M6 passed 94 tests; M7 passed 99 tests; M8 passed 104 tests.
At 100 columns, the card-budget checks allow 30 lines for a function and 42
for a class. The M8 sweep read the local database only; no network access was
needed. Preserve offline page viewing and search. See `docs/roadmap.md` for
scope and limitations.

A manual-page quality audit of v0.2.0 is recorded in
`docs/audit-2026-11-manual-quality.md`. M1 added attributed real-page
fixtures; M2 fixed parser loss of `t-li1` descriptions, declaration
metadata, nested revision rows, and member-name boundaries. A strict
real-fixture invalidation assertion now passes through the M3 quick card.
The owner set the product direction in
`docs/quick-reference-spec.md`: a short "quick card" default page with
everything else behind `--full`, task-oriented discovery, and coverage of
the whole C++ standard library. M3 quick-card projection and rendering are
complete. M4 added SQLite record format 3. M5 adds full/example display,
ranked purpose-aware search, indexed alias resolution, better not-found
guidance, and quiet broken-pipe handling. M6 adds the generated catalog and
header index. M7 adds resumable, per-page full-catalog ingestion with retries
and failure reports. M8 adds a read-only coverage sweep and report. The latest
sweep covers 2,167 local pages with no page/category findings; see
`docs/coverage-report.md`. Keep per-page updates and offline read boundaries
intact.

Follow-up continued on 2026-11-22 at the owner's request. The three previously
stale pages (`array`, `vector`, and `vector::push_back`) were refreshed by
targeted updates and now use stored format 3. The catalog builder was fixed to
exclude experimental/TS routes and library overview pages while retaining the
substantive Library feature-test-macros reference. The regenerated catalog has
2,496 names across 111 headers and 2,167 unique routes; the original 50
default-update pages remain. A catalog check found zero excluded routes.
`cxxman update --resume` reported 386 updated, 0 failed, and 2,120 skipped;
two missing routes and the feature-test-macros page were then refreshed.
Legacy experimental, overview, header-index, and compiler-support cache rows
outside the catalog were removed.

The read-only coverage sweep is recorded in `docs/coverage-report.md`: all
2,167 catalog routes are stored and swept, with zero page/category findings.
Four pages have no source purpose sentence; their NAME lines show only the
title. There are no stored records outside the generated catalog. The sweep
checks parsed structure and rendered card budgets, not every technical fact
against live cppreference content.
Follow M1–M8 in order.

Before starting, inspect Git status, read `AGENTS.md`, this handoff, and
the relevant roadmap and developer-guide sections. Do not restart the
initial CLI-skeleton task or the completed release setup.
