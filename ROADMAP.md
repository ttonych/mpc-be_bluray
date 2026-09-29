# Roadmap

[Русский](ROADMAP.ru.md) · [Overview](README.md) · [Changelog](docs/en/CHANGELOG.md)

The project adapts Blu-ray menus in MPC-BE for use with the **madVR video renderer**.
The first Windows x64 portable [prerelease is available](https://github.com/ttonych/mpc-be_bluray/releases/tag/1.9.1-bluray.1)
for testing alongside an existing MPC-BE. Priorities may change with feedback;
items below are not promised release dates or compatibility guarantees.

## Current baseline

- [x] Port the integration to the official MPC-BE 1.9.1 release base.
- [x] Build MPC-BE, Russian resources, libbluray 1.5.0 and its two JARs locally,
  with the maintained patches applied.
- [x] Run component, local-patch and publication-checker tests.
- [x] Check HDMV with Baby Boom and BD-J with Ford v Ferrari on the local 1.9.1 build.
- [x] Prepare the isolated portable profile and optional existing-settings import.
- [x] Prepare English/Russian documentation and manual GitHub build configuration.

The source is public and `main` is protected. The
[first cloud build](https://github.com/ttonych/mpc-be_bluray/actions/runs/35601595821)
passed, and its ZIP integrity and clean profile were checked. The later branded
cloud candidate passed user checks on A Knight’s Tale (BD-J) and Baby Boom (HDMV).
Grand Prix exposed missing separate menu audio and a failed-read recovery crash;
these fixes are included in cloud candidate 35619821494. Its package and
RU/EN profile checks passed, and the tester confirmed all three discs.
That same ZIP was published as prerelease 1.9.1-bluray.1.

## 1.9.1-bluray.1 release checks

- [x] Add the fork version to the player, EXE properties, ZIP and manifest;
  verify the Git revision and use fork releases for update checks.
- [x] Include the bilingual user instructions in the ZIP and check its offline links.
- [x] Complete first-run UI checks in both languages: registry and detected INI
  availability, manual INI selection, switching sources, defaults and saved settings.
- [x] Recheck cloud-package isolation with registry and synthetic INI imports,
  preserving source profiles; the earlier local update check also preserved the INI.
- [x] Review the proposed initial history, public author identity, licenses,
  source/patch provenance and absence of personal data; create the public repository
  and establish the reviewed `main` branch. Require PRs and passing checks, block
  direct pushes/history rewriting, and do not require a second person's approval.
- [x] Add quick PR checks for documentation links/examples and RU/EN string IDs,
  format arguments and dialog controls; verify their error detection locally.
- [x] Run the full GitHub Actions build with dependencies prepared on the hosted
  runner; inspect the ZIP contents and hashes.
- [x] Prepare a draft Release for the build commit, with its existing ZIP, checksum,
  component versions and bilingual release notes; keep it unpublished during validation.
- [x] Test the actual cloud-built ZIP with madVR: HDMV, BD-J, film start, menu
  recall and a clean first-run profile; Grand Prix menu audio and pause/resume.
  Record the exact scope; earlier track/chapter checks used the previous candidate.
- [x] Publish the draft as a prerelease with the same tested ZIP, checksum and
  matching source tag, without rebuilding or replacing the validated artifact,
  with download links and reporting instructions in both languages.

Release readiness means the remaining checks pass and the documentation matches
the artifact users receive. A locally working EXE alone is not the release gate.

Local candidate checks on 2026-09-21 passed: RU/EN first-run and About windows,
registry-found / automatic-INI-missing display, manual file selection and cancellation,
source switching, defaults and saved settings. Registry import left source values
and key timestamps unchanged; manual import retained preferences while clearing
history and shared BD-J paths. Replacing program files preserved the test INI and
did not repeat setup. A private UI host using the production dialog class and
compiled resources checked no sources, detected INI only, and both sources, with
process-local registry redirection and synthetic profiles. Source files were
preserved and all temporary registry keys were removed.

The replacement cloud candidate 35619821494 passed RU/EN first-run and About
checks, setup cancellation, registry import and manual synthetic-INI import.
Source profiles remained unchanged; imported history and shared BD-J paths were
cleared. Its commit and checksum are recorded in the development guide and
published Release. The tester confirmed menus, film start and menu recall on
A Knight’s Tale and Baby Boom, plus separate music, pause/resume and film start
on Grand Prix. The tested ZIP was published without rebuilding.

## Preparing 1.9.1-bluray.2

- [x] Implement local fixes for multi-file read corruption and faded HDMV HDR
  graphics; verify the affected discs with madVR and user confirmation.
- [x] Add shared Java 21 x64 selection and show the found or running runtime;
  preserve explicit settings whose translated labels match the default.
- [x] Merge the reviewed source and complete the manual cloud build.
- [x] Check the exact cloud ZIP: HDMV, BD-J, Java selection, RU/EN and a fresh
  portable profile. Earlier local tests are not a substitute for this check.
- [x] Prepare a draft with bilingual notes, the verified ZIP and its checksum.
- [ ] Publish that same ZIP as the second prerelease.

The [cloud candidate from run 36602524325](https://github.com/ttonych/mpc-be_bluray/actions/runs/36602524325)
passed compilation, component tests, package checks and bounded playback/UI
validation on 2026-09-29. Streets of Fire covered HDMV HDR graphics and navigation;
Baby Boom covered direct ISO startup, film/menu transitions and mount lifetime;
Casino Royale (2006) covered BD-J startup, film and menu recall with Java 21.
RU/EN first-run and Java controls passed; the source registry remained unchanged.
The draft is ready; publication is still pending. Exact build identity, checksum
and limits are recorded in the [development guide](docs/en/DEVELOPMENT.md#second-prerelease-candidate).

The published first-release ZIP remains unchanged. Detailed changes are in the
[changelog](docs/en/CHANGELOG.md#191-bluray2).

- Fixed lost playmarks during BD-J menu returns; local tests and five menu
  returns with restored graphics passed. The unsafe process-state write also
  corrected in that change belongs to a disabled hook and was not the cause
  of the observed crashes. A later investigation identified a native multi-file
  read buffer overrun; guarded reads and repeated BD-J startup/menu/film checks
  passed after its separate correction. The cloud ZIP passed the bounded
  qualification above; the original crash disc was not repeated in that run.
- Fixed Blu-ray ISO images being unmounted between playlists. Local checks with
  Baby Boom and madVR covered the introductory clips, menu, film start, return to
  the menu, the Open ISO dialog, and releasing the image on disc/player close.
  The tester also confirmed both introductory clips, the menu and film start
  after dragging the ISO onto the corrected player.
  The published 1.9.1-bluray.1 ZIP does not contain this fix. The second cloud
  candidate passed direct ISO opening, both introductory clips, menu, film,
  top-menu return and release of the image when closing the disc.

## After initial feedback

- Prioritize reproducible menu, playback, data-isolation and crash regressions.
- Maintain a compact compatibility record that names the tested player build,
  madVR/Java versions, disc edition and exact scenarios rather than declaring a
  whole disc fully supported after a short check.
- Repeat window scaling, menu/still transitions, gallery and disc-change checks
  where reports identify gaps. Revisit unconfirmed mouse wake-up from BD-J screensavers.
- Merge selected official MPC-BE releases through update branches, keeping the
  separate fork revision number and local-first development workflow.
- Evaluate new libbluray and Java 21 updates against the existing regressions.
  Keep, port or retire local patches according to upstream behavior and test results.

## Deferred or not scheduled

| Topic | Status |
| --- | --- |
| Local diagnostic report and a link to project issues | Deferred to the next work session; madVR crash-handler customization is a separate investigation |
| Seamless transitions between playlists | Deliberately deferred; retain the known limitation until this work is resumed |
| Standard profile / installer distribution | Revisit after the portable trial; preserve the existing build option |
| BD-Live, PiP, button sounds, Java video layout and full pause synchronization | Known gaps; each needs a separate scope and validation plan |
| Full MVC/stereoscopic output and broader HDR validation | Menu/start tests do not establish these; no completion claim |
| Other video renderers or an MPC-HC port | Outside the current madVR-focused MPC-BE release scope |
| Java 25+ | No upgrade commitment while the current BD-J runtime has the SecurityManager incompatibility |

Track completed changes in the changelog. Keep this file and its Russian
translation synchronized as priorities and verified results change.
