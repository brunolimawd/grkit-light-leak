# Contributing

This repository follows [GRKit's contribution rules](https://github.com/ahonn/grkit/blob/main/CONTRIBUTING.md)
where they apply to an external module. See [AGENTS.md](AGENTS.md) for
the module's compatibility rules and the pinned SDK documentation.

## Checks

Install the pinned `grkit` CLI and the compilers as described in
[README.md](README.md#build-from-source). From this repository:

```sh
grkit module test --dir .
mkdir -p build
grkit module build --dir . --out build/LITLEAK.GRM
```

The compiler refuses to overwrite a module or its intermediate directory.
For another build, choose a fresh directory under `build/`.

For the sanitizer and installer contract checks, point `GRKIT_SOURCE` at
an upstream source checkout whose `HEAD` matches `GRKIT_REV`. This checkout
is only needed for the check scripts, not for normal builds.

```sh
GRKIT_SANITIZER_CC=clang UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  grkit module test --dir . --cc "$GRKIT_SOURCE/.github/scripts/clang-sanitized.sh"
clang -I "$GRKIT_SOURCE/runtime" \
  "$GRKIT_SOURCE/tests/runtime/grm_registrations_contract.c" \
  -o build/check-registrations
build/check-registrations build/LITLEAK.GRM
```

The last command must print `status 0`. CI builds and checks the `.GRM`
with LLVM 21 on Linux. Host tests and the installer check do not replace
a physical camera test.

## Icons

The locally generated 32×32 viewfinder masks follow the
[GRKit icon guidelines](https://github.com/ahonn/grkit/blob/61dd9a697668d809939c2a75d402d76e418ff6fc/docs/icon-guidelines.md).
The generator uses only the Rust standard library. Run:

```sh
mkdir -p build
rustc --edition=2024 tools/generate-badge.rs -o build/generate-badge
build/generate-badge badge_art.h
```

Review them at native size on light and dark backgrounds. The ADJ PNG
tiles are approved source artwork; the viewfinder generator does not
rewrite them.

## Pull requests and releases

- Use Conventional Commits. Describe the user-visible change, list the
  checks you ran and record the camera model and firmware for hardware
  tests. If you didn't test on camera, say so.
- For a release, update the public SemVer in `VERSION` and record the
  user-visible changes and actual camera test results in `CHANGELOG.md`.
  Increase the integer `version` in `module.toml` when releasing a changed
  `.GRM`; the SDK does not accept a SemVer string there. Build from a clean
  checkout. Use `v` followed by `VERSION` for the tag, mark beta releases
  as pre-releases, and attach `LITLEAK.GRM`. Do not commit the build output.
- Keep the MIT license and notices for any reused material. Do not add
  firmware or third-party reference photos.
