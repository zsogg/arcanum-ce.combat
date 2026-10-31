# Setup tools for the macOS package

The macOS package's `setup.sh` unpacks two Windows installers: GOG's
Arcanum installer (Inno Setup) and the Unofficial Arcanum Patch (NSIS).
Since September 2026 Homebrew's installer refuses Intel Macs, so the package
carries its own two unpackers in a `tools` folder, built here by the macOS
workflow's `tools` job (`.github/workflows/macos-build.yml`). Both are
universal binaries, signed ad hoc, for macOS 10.13 on Intel and 11.0 on
Apple Silicon. `setup.sh` uses a `tools` folder before anything on `PATH`,
on any system, and on macOS clears the folder's quarantine flag first.

## gogx: GOG's installer

`gogx/` is a small Rust program on the [innospect](https://crates.io/crates/innospect)
crate, which reads Inno Setup installers. innoextract does the same job but
needs Boost, which no longer builds for Intel Macs without Homebrew.

```console
gogx INSTALLER DIR        extract into DIR, which must be new or empty
gogx --test INSTALLER     decode and check every file, write nothing
gogx --version
```

Its output is innoextract's with `--exclude-temp`: the same paths and the
same bytes for GOG's Arcanum 1.0.7.4 installer (build 77721, Inno Setup
5.6.2), checked on Linux on 2026-10-31, and for every installer in
innospect's test set that innoextract can read (Inno Setup 5.0.8 to 6.0.0).
It also reassembles the "GOG Galaxy" parts GOG's newer installers split
their files into, as innoextract does. Every file is checked: Inno Setup's
own checksum of each stored file, and GOG's MD5 of each reassembled one.
Anything it does not understand is an error, never a skipped file:

- an installer whose data is in separate `.bin` files: setup's message then
  points at installing the game on a Windows PC and giving setup the folder;
- an encrypted installer, or one older than Inno Setup 4.0.9;
- a path that would leave the output folder, or two files with one name
  (compared without case, as a Mac's disk compares them).

innospect 0.2.0 needs three fixes for GOG's installers, kept in
`gogx/patches/` until a release carries them. `gogx/prepare.py` fetches the
published crate, checks its sha256 against the one crates.io publishes
(pinned in the script), and applies them exactly, with no fuzz, into
`gogx/vendor/`, which `Cargo.toml` points at:

| Patch | Fixes |
|---|---|
| 0001 | A chunk stored uncompressed was decoded with the installer's compression; GOG's 1.0.7.4 installer failed on 191 of 317 entries. |
| 0002 | Installers from Inno Setup 5.6's ANSI builds were read as UTF-16. |
| 0003 | The 6.4.0 pre-release's header was read five flags short. |

Building, here or on a Mac (Rust from rustup.rs; `rust-toolchain.toml` pins
the version):

```console
gogx/build.sh          # this machine: gogx/target/release/gogx
gogx/build.sh --mac    # on a Mac: universal, signed, gogx/target/gogx
cd gogx && cargo test --release --locked
```

Peak memory on GOG's 1.2 GB installer is about 1.1 GB of the program's own,
plus the installer itself mapped from disk.

## 7zz: the patch's installer

`7zip/build.sh` builds 7-Zip 26.05's console program from 7-Zip's own source
release, checked by sha256, with one patch. 7-Zip's prebuilt Mac binary is
not used: its Intel slice requires macOS 10.15. The cause is one call,
`timespec_get()`, which Apple added in 10.15. `7zip/patches/` makes the Mac
build use `clock_gettime()`, in macOS since 10.12, and the build treats any
call of an API newer than its target as an error. It runs on a Mac only
(`make`, `lipo`, `codesign`) and writes `7zip/build/7zz`.

## Licences

gogx is part of Arcanum CE. `gogx/licenses.py` prints the licence notices of
every crate compiled into it, from `Cargo.lock`; the workflow joins them
with 7-Zip's `License.txt` and `unRarLicense.txt` into the package's
THIRD-PARTY-NOTICES.md. 7-Zip is under the GNU LGPL with the unRAR
restriction; its source is the release named in `7zip/build.sh`, and our
change to it is `7zip/patches/`. Each package's `tools/SOURCES.txt` names
what its binaries were built from.
