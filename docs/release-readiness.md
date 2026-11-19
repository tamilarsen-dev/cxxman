# GitHub Release Readiness

This file tracks completed local work, the public GitHub repository, and
remaining steps before the first downloadable release.

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
- The Windows ZIP, PowerShell installer/uninstaller, and SHA-256 checksum
  were assembled and checked locally.
- POSIX installer and uninstaller scripts passed Bash syntax validation
  and an isolated install/remove check using a temporary home directory.
- Release archive packaging tests pass for ZIP and POSIX tar archives.
- The latest full Python test run passed: 34 tests.
- `.gitignore` excludes virtual environments, test/build output, and local
  documentation databases.
- The 15 existing commits were rewritten to use the GitHub noreply address
  for author and committer metadata before the public push. A local-only
  backup branch retains the original history; it was not pushed.

## Configured but not verified remotely

`.github/workflows/release.yml` runs on pushes and pull requests. It
builds and smoke-tests native Windows x64, Linux x64, Intel macOS, and
Apple Silicon macOS binaries. A matching `v*` tag publishes four
platform archives and checksum files in one GitHub Release.

Only the Windows build has been executed in this workspace. The first
GitHub Actions run for the public `master` push is queued; Linux and macOS
still need successful remote builds before publishing the first release.

## Still needed

1. Check the cross-platform GitHub Actions matrix and fix any failures.
2. Choose a project license. No `LICENSE` file has been added because the
   license is an owner decision.
3. After the matrix passes, publish the first version tag and verify the
   resulting release assets and checksums.

macOS executables are not signed or notarized. The Linux x64 build uses
Ubuntu 22.04 as its compatibility baseline. These are documented in
`docs/release.md`.
