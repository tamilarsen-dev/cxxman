# Project Handoff

Last updated: 2026-11-20

## Current state

- Public repository: <https://github.com/tamilarsen-dev/cxxman>
- Latest release: [`v0.1.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.1.0)
- The release includes Windows x64, Linux x64, Intel macOS, and Apple
  Silicon macOS archives, plus SHA-256 sidecar files.
- GitHub Actions passed the test, smoke-test, and packaging jobs for all
  four platforms. The tag workflow published the release successfully.
- MIT license is in the root `LICENSE` file.
- Local test suite last run: 34 passed. The Windows standalone executable
  was also smoke-tested locally.
- macOS binaries are not signed or notarized. Linux x64 uses Ubuntu 22.04
  as the compatibility baseline.

## Git state at handoff

- Branch: `master`, tracking `origin/master`.
- Last completed commit before this handoff note: `290e917`.
- The original commit author/committer addresses were rewritten to the
  account's GitHub noreply address before the public push. A local backup
  branch preserves the original history; do not push that backup branch.
- Stages have been committed locally throughout this project. Continue
  committing each coherent stage.

## Next planned work

The first distribution milestone is complete. Continue with the post-MVP
direction in `docs/roadmap.md`: expand the curated library-function
manuals beyond `std::vector::push_back`. Inspect the current catalog and
tests before choosing the next function page. Keep each addition small and
fixture-backed, and preserve offline page viewing and search.

Before starting, inspect Git status, read `AGENTS.md`, this handoff, and
the relevant roadmap and developer-guide sections. Do not restart the
initial CLI-skeleton task or the completed release setup.
