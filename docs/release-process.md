# Product release process

NekoSpace Audio is a monorepo, but its products do not share one public version number.
A Binaural bug fix must not make an unchanged Reverb appear to have a new release, and a
first Reverb alpha cannot reuse Binaural's existing `v0.1.0-alpha` tag.

## Version and tag ownership

| Product | Version source | Tag pattern | Example |
| --- | --- | --- | --- |
| NekoSpace Binaural | top-level `project(... VERSION ...)` | `v<version>` | `v0.2.0-alpha` |
| NekoSpace Reverb | `NEKOSPACE_REVERB_VERSION` in its CMake file | `reverb-v<version>` | `reverb-v0.1.0-alpha` |

The prerelease suffix belongs to the Git tag and GitHub Release. JUCE's binary version is
the numeric core (`0.1.0`), because its platform version fields are numeric.

Existing Binaural tags remain unchanged. New products must use a product prefix rather
than taking ownership of the legacy unprefixed namespace.

## Release workflow

A matching tag runs `.github/workflows/release.yml`. The workflow:

1. identifies the product from the tag and rejects unknown tag forms;
2. configures the pinned JUCE tree on Windows 2022;
3. builds only that product's deliverables and required tests;
4. runs the product's CTest group;
5. checks that the tag's numeric version matches the product's CMake version;
6. creates one Windows x64 zip with the applicable README, changelog, AGPL licence and
   third-party notices;
7. records product, tag, binary version, commit and dirty state inside the archive, then
   emits a SHA-256 checksum beside it;
8. opens a **draft** GitHub Release using that product's changelog section.

Draft creation is not publication. The draft is inspected before the irreversible public
action. No tag is pushed while a binding release gate is failed or undocumented.

## Package contents

| Product | Executables |
| --- | --- |
| Binaural | VST3 bundle and JUCE Standalone |
| Reverb | VST3 bundle, JUCE Standalone and Reverb Player |

The Reverb Player is included because it is the supported file-audition path and hosts the
exact shipping processor/editor. Analyzer binaries, tests, private recordings, generated
video/audio and local benchmark output are never release assets.

## Reverb first-alpha gate

The intended first tag is `reverb-v0.1.0-alpha`, but the tag is not permission to skip
[Reverb validation](../plugins/reverb/docs/validation.md). Before it is created:

- every binding engineering gate must pass or carry an explicit, reviewed waiver;
- the permanent parameter/state contract and migration baseline must be frozen;
- pluginval, Steinberg Validator and the declared host smoke tests must be current;
- the locally produced zip must be inspected for exact contents and private-media absence;
- the changelog date and release notes must describe the shipped build.
