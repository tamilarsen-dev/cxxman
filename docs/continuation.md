# Project Handoff

Last updated: 2026-11-22

## Current state

- Public repository: <https://github.com/tamilarsen-dev/cxxman>
- Latest release: [`v0.3.1`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.3.1)
- The release provides Windows x86 and x64, Linux x64, Intel macOS, and
  Apple Silicon builds. Both Windows setup executables passed CI install and
  uninstall smoke checks.
- The v0.3.1 tag workflow published all seven build assets and their seven
  SHA-256 sidecars. All 14 download URLs returned HTTP 200 after publishing.
- Standalone builds bundle the generated 2,496-name catalog (111 headers,
  2,167 unique cppreference routes). Users explicitly download page content
  with `cxxman update`; viewing and search remain local and offline.
- The full local test suite passes 108 tests. The successful branch build
  run is [45007022878](https://github.com/tamilarsen-dev/cxxman/actions/runs/45007022878);
  the tagged release run is [45007467635](https://github.com/tamilarsen-dev/cxxman/actions/runs/45007467635).
- macOS binaries are not signed or notarized. Linux x64 uses Ubuntu 22.04
  as its compatibility baseline. Windows ARM and Linux ARM builds are not
  included.
- MIT license is in the root `LICENSE` file.

## Git and release state

- Branch: `master`, tracking `origin/master`.
- Tag `v0.3.1` points to release commit `f386898` (`Make architecture tests
  deterministic`). The preceding implementation commit is `8ea0f25`.
- README and project status documents describe the published v0.3.1 release.
- Keep the public release tag and the historical `v0.1.0` through `v0.3.0`
  releases intact.

## Completed implementation and validation

Milestones M1–M8 in `docs/quick-reference-spec.md` are complete: real-page
fixtures, parser fidelity, quick-card projection/rendering, stored format 3,
CLI detail/example options, generated catalog/header index, resumable
full-catalog ingestion, and read-only coverage validation. The current suite
passes 108 tests. Keep offline page viewing and search boundaries intact.

The catalog builder excludes experimental/TS routes and library overview
pages while retaining substantive Library reference topics. The generated
catalog has 2,496 names across 111 headers and 2,167 unique routes. The
read-only coverage sweep found all catalog routes stored and no page/category
findings. Four pages have no source purpose sentence and therefore show only
their title in the NAME section. This is structural coverage, not a manual
technical audit of every fact against live cppreference content. See
[`coverage-report.md`](coverage-report.md).

A manual-page quality audit of v0.2.0 is preserved in
[`audit-2026-11-manual-quality.md`](audit-2026-11-manual-quality.md).
Attributed real cppreference fixtures and parser fixes address the earlier
loss of `t-li1` descriptions, declaration metadata, nested revision rows,
member-name boundaries, and invalidation details. The approved quick-card
format and full-library scope are in
[`quick-reference-spec.md`](quick-reference-spec.md).

The catalog was regenerated after excluding out-of-scope routes. A resumable
update completed with 386 pages updated, zero failures, and 2,120 skipped;
subsequent targeted updates refreshed missing routes and the feature-test
macros page. Legacy experimental, overview, header-index, and
compiler-support cache rows outside the catalog were removed.

## Next work

Before starting another milestone, inspect Git status, `AGENTS.md`,
`docs/quick-reference-spec.md`, `docs/roadmap.md`, and the manual-quality
audit. Work on the first incomplete item in the current work queue; do not
restart the original CLI skeleton, completed M1–M8 work, or release setup.

A prebuilt or downloadable documentation database still needs explicit owner
approval and a plan for cppreference CC-BY-SA 3.0 / GFDL attribution and
share-alike terms. Do not treat the catalog bundled in v0.3.1 as a database:
it lists topics, while users fetch page content with explicit update
commands.
