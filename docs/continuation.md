# Project Handoff

Last updated: 2026-11-20

## Current state

- Public repository: <https://github.com/tamilarsen-dev/cxxman>
- Latest release: [`v0.2.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.2.0)
- The release includes Windows x64, Linux x64, Intel macOS, and Apple
  Silicon macOS archives, plus SHA-256 sidecar files.
- GitHub Actions passed the test, smoke-test, and packaging jobs for all
  four platforms. The tag workflow published the release successfully.
- MIT license is in the root `LICENSE` file.
- Published `v0.2.0` passed its release test, packaging, and installer
  smoke-test workflows. The current suite passes 57 tests.
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
- The working tree is clean after this documentation sync.

## Next planned work

The first distribution milestone, the initial manual-page projection
slice, the 50-page default catalog, app updater, Windows installer, and
`v0.2.0` release are complete. Six representative pages have saved fixtures
for manual projection tests; remaining catalog pages use the conservative
source-heading projection. Expand entity-specific extraction only with
representative fixtures and source-accuracy tests. Preserve offline page
viewing and search. See `docs/roadmap.md` for scope and limitations.

Before starting, inspect Git status, read `AGENTS.md`, this handoff, and
the relevant roadmap and developer-guide sections. Do not restart the
initial CLI-skeleton task or the completed release setup.
