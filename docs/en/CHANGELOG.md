# Fork changelog

[Русский](../ru/CHANGELOG.md) · [Project overview](../../README.md)

This file covers this fork. The original MPC-BE history remains in
[Changelog.txt](../Changelog.txt).

## 1.9.1-bluray.1

Base: official MPC-BE 1.9.1; libbluray 1.5.0 with local `mouse-page-v1` and
`bdj-toggle-v1` patches. See the release notes for publication status and ZIP validation results.

- Added HDMV and BD-J menu navigation, adapted for use with the madVR video renderer.
- Added menu/top-menu controls, disc track selection, chapter marks and OSD.
- Corrected mouse coordinates with letterboxing and added a restricted HDMV
  submenu return path for compatible menu authoring patterns.
- Added startup menu graphics and still-image handling; improved gallery timing,
  RLE validation, graphics lifetime handling and HDR menu brightness.
- Added disc-change detection, disc title updates and filtering of chapter marks
  in menus/background playback.
- Added Blu-ray settings tabs and a readable disc-data list, with reset and rename.
- Added external Java configuration; tested with Temurin JRE 21.0.12.1+1 x64.
- Added isolated portable profiles and optional settings import, with Russian
  and English interface resources.
- Added the fork version to the title, About dialog and EXE properties; update
  checks use this fork, including published prereleases.
- Added offline English/Russian HTML instructions to the ZIP.
- Added maintained libbluray patches, component tests, publication checks,
  manual GitHub Actions build configuration and bilingual documentation.

The tested video renderer is madVR 210 x64: update 210 applied over the base
madVR package. See the [user guide](USAGE.md#setup-and-madvr) for both downloads
and the installation order.

Local playback checks on the official 1.9.1 base: Baby Boom (HDMV) and Ford v
Ferrari (BD-J). Earlier development checks also covered selected BD/UHD/3D-disc
menus, stills and navigation; they are not a complete retest on this base.
The first GitHub Actions build passed compilation, component tests and package
integrity checks. Playback validation belongs to each specific release ZIP;
see its release notes.

Seamless playlist transitions, BD-Live, PiP playback and full stereoscopic output
validation remain outside this release's completed work. See the
[current limits](../../README.md#current-limits).
