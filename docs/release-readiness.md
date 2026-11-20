# GitHub Release Readiness

This file records completed local work, the public GitHub repository, and
the first downloadable release.

## Repository setup

| Setting | Planned value |
|---|---|
| Owner | `tamilarsen-dev` |
| Repository | `cxxman` |
| Visibility | Public, for downloadable releases and source access |
| Description | C++ reference manuals in your terminal, with offline search and on-demand updates. |
| Initial branch | `master` |

The public repository is live at
[`tamilarsen-dev/cxxman`](https://github.com/tamilarsen-dev/cxxman), with
the description shown above. The local `master` branch is pushed and
tracks `origin/master`. GitHub CLI is not installed; the repository was
created through the browser.

The original history contained a non-GitHub-noreply author address. Before
the public push, author and committer addresses were rewritten to the
account's noreply address. The resulting commit hashes differ from the
original local history.

## Completed locally

- CLI commands, supported-topic discovery, first-run guidance, and
  actionable search/update messages are implemented.
- User and developer manuals are in `docs/user-manual.md` and
  `docs/developer-guide.md`.
- A Windows x64 one-file executable was built with Python 3.15.2 and
  PyInstaller 6.22.3. Its help, version, catalog, offline search/list,
  and page rendering passed the executable smoke check.
- The published `v0.1.0` Windows ZIP was assembled and checked locally.
- The current release workflow builds an Inno Setup Windows installer
  with a generated uninstaller and runs an install/remove smoke test.
- POSIX installer and uninstaller scripts passed Bash syntax validation
  and an isolated install/remove check using a temporary home directory.
- Release archive packaging tests pass for ZIP and POSIX tar archives.
- The `v0.1.0` release-era full Python test run passed: 34 tests.
- `.gitignore` excludes virtual environments, test/build output, and local
  documentation databases.
- The MIT license is selected and included in the public repository.
- The 15 existing commits were rewritten to use the GitHub noreply address
  for author and committer metadata before the public push. A local-only
  backup branch retains the original history; it was not pushed.

## Configured and verified remotely

`.github/workflows/release.yml` runs on pushes and pull requests. It
builds and smoke-tests native Windows x64, Linux x64, Intel macOS, and
Apple Silicon macOS binaries. A matching `v*` tag publishes four
platform archives and checksum files in one GitHub Release.

The Windows executable was built and smoke-tested locally. Two branch
workflow runs passed all four native builds. The `v0.1.0` tag workflow also
passed Windows x64, Linux x64, Intel macOS, and Apple Silicon macOS, then
successfully published the GitHub Release.

## Published release

[`v0.1.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.1.0)
is the latest public release. It contains four platform archives, four
SHA-256 sidecar files, and GitHub's source archives. The README links to
the latest release, and `docs/release.md` describes platform installation.

The current source tree has since added a 50-page default topic catalog,
manual-page projection, page color/syntax-highlighting controls, and
application update commands. The current test suite passes 57 tests. These
newer changes have not yet been included in a published binary release;
the `v0.1.0` executable retains its earlier catalog.

The current working tree adds the per-user Windows setup executable and
application update commands; those changes are not part of published
`v0.1.0`. A future `v*` tag will publish the Windows setup `.exe`, release
archives, and checksums through the updated workflow.

macOS executables are not signed or notarized. The Linux x64 build uses
Ubuntu 22.04 as its compatibility baseline. These are documented in
`docs/release.md`.
