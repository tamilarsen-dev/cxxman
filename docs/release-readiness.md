# GitHub Release Readiness

This file tracks what has been checked locally and what still needs the
GitHub repository and Actions.

## Repository setup

| Setting | Planned value |
|---|---|
| Owner | `tamilarsen-dev` |
| Repository | `cxxman` |
| Visibility | Public, for downloadable releases and source access |
| Description | C++ reference manuals in your terminal, with offline search and on-demand updates. |
| Initial branch | `master` |

The GitHub account is signed in through the browser. The repository has
not yet been created, and this checkout has no Git remote. GitHub CLI is
not installed; repository creation will use the already-open GitHub page.

Before the first public push, decide whether to rewrite the local commit
author metadata to the account's GitHub noreply address. One author address
in the existing history is not a GitHub noreply address, so pushing the
history unchanged would make that address visible in public commit data.
Rewriting preserves the staged work but changes commit hashes.

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

## Configured but not verified remotely

`.github/workflows/release.yml` runs on pushes and pull requests. It
builds and smoke-tests native Windows x64, Linux x64, Intel macOS, and
Apple Silicon macOS binaries. A matching `v*` tag publishes four
platform archives and checksum files in one GitHub Release.

Only the Windows build has been executed in this workspace. Linux and
macOS builds still need a successful GitHub Actions run before publishing
the first release.

## Still needed

1. Create public `tamilarsen-dev/cxxman` without GitHub-generated README,
   `.gitignore`, or license files; the local repository already has its
   own README and `.gitignore`.
2. Add `origin` and push the existing `master` branch.
3. Check the cross-platform GitHub Actions matrix and fix any failures.
4. Choose a project license. No `LICENSE` file has been added because the
   license is an owner decision.
5. After the matrix passes, publish the first version tag and verify the
   resulting release assets and checksums.

macOS executables are not signed or notarized. The Linux x64 build uses
Ubuntu 22.04 as its compatibility baseline. These are documented in
`docs/release.md`.
