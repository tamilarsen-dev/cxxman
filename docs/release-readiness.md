# GitHub Release Readiness

Last updated: 2026-11-22

## Repository

- Public repository: <https://github.com/tamilarsen-dev/cxxman>
- Current release: [`v0.3.1`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.3.1)
- License: MIT, in the root `LICENSE` file.
- Release workflow: [GitHub Actions](https://github.com/tamilarsen-dev/cxxman/actions/workflows/release.yml)

## v0.3.1 builds

The release workflow builds natively on five targets:

| Target | Archive | Windows setup |
|---|---|---|
| Windows x64 | `cxxman-windows-x64.zip` | `cxxman-windows-x64-setup.exe` |
| Windows x86 | `cxxman-windows-x86.zip` | `cxxman-windows-x86-setup.exe` |
| Linux x64 | `cxxman-linux-x64.tar.gz` | — |
| Intel macOS | `cxxman-macos-x64.tar.gz` | — |
| Apple Silicon macOS | `cxxman-macos-arm64.tar.gz` | — |

Every archive and Windows setup executable has a SHA-256 sidecar. All 14
release assets (seven files and their seven checksum sidecars) returned HTTP
200 when checked after publication. The release includes the generated
catalog with 2,496 names across 111 headers, mapped to 2,167 cppreference
routes in the supported `/cpp/` scope.

## Validation

- The local Python 3.15.2 suite passes **108 tests**.
- The successful branch workflow built all five targets; the Windows x86 job
  passed its tests, executable smoke check, installer build, install/remove
  smoke check, packaging, and asset upload.
- The tagged release workflow published the v0.3.1 assets after its build
  matrix completed. See the [v0.3.1 workflow run](https://github.com/tamilarsen-dev/cxxman/actions/runs/45007467635).
- Both Windows setup programs install for the current user and generate an
  uninstaller. CI verifies installation, PATH changes, and removal.

## Release history and limitations

[`v0.1.0`](https://github.com/tamilarsen-dev/cxxman/releases/tag/v0.1.0)
was the first public release. `v0.2.0` added the Windows setup, application
update commands, and checksums. `v0.3.0` bundled the generated topic catalog
in standalone builds. `v0.3.1` adds Windows x86 binaries and installers.

The executable bundles the topic catalog, not downloaded page contents;
users run `cxxman update` to populate the local database. The catalog is a
curated C++ standard-library scope, not all of cppreference. macOS binaries
are not signed or notarized. Linux x64 builds use Ubuntu 22.04 as the
compatibility baseline. Windows ARM and Linux ARM builds are not included.
See [`release.md`](release.md) for installation and update instructions.
