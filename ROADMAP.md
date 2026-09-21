# Roadmap

[Русский](ROADMAP.ru.md) · [Overview](README.md) · [Changelog](docs/en/CHANGELOG.md)

The project adapts Blu-ray menus in MPC-BE for use with the **madVR video renderer**.
The immediate goal is a public Windows x64 portable prerelease that users can
test alongside their existing MPC-BE. Priorities may change with feedback;
items below are not promised release dates or compatibility guarantees.

## Current baseline

- [x] Port the integration to the official MPC-BE 1.9.1 release base.
- [x] Build MPC-BE, Russian resources, libbluray 1.5.0 and its two JARs locally,
  with both maintained patches applied.
- [x] Run component, local-patch and publication-checker tests.
- [x] Check HDMV with Baby Boom and BD-J with Ford v Ferrari on the local 1.9.1 build.
- [x] Prepare the isolated portable profile and optional existing-settings import.
- [x] Prepare English/Russian documentation and manual GitHub build configuration.

These are local preparation results. The public repository, first cloud build
and first published package are still pending.

## Before 1.9.1-bluray.1

- [ ] Finish the fork version label in the player and align it with the ZIP,
  manifest, tag and release notes.
- [ ] Include the bilingual user instructions in the ZIP and check its offline links.
- [ ] Complete first-run UI checks in both languages: registry and detected INI
  availability, manual INI selection, switching sources, defaults and saved settings.
- [ ] Recheck the clean package's profile isolation with both installed and
  portable source profiles; check importing and updating without changing source data.
- [ ] Review the proposed initial history, public author identity, licenses,
  source/patch provenance and absence of personal data; create the public repository
  and establish the reviewed `main` branch. Require PRs and passing checks, block
  direct pushes/history rewriting, and do not require a second person's approval.
- [ ] Extend quick PR checks with RU/EN resource consistency and documentation
  validation; currently only publication inputs and the checker itself are automated.
- [ ] Run the full GitHub Actions build with dependencies prepared on the hosted
  runner; inspect the ZIP contents and hashes.
- [ ] Prepare a draft Release for the build commit, with its existing ZIP, checksum,
  component versions and bilingual release notes; keep it unpublished during validation.
- [ ] Test that actual cloud-built ZIP with madVR: HDMV, BD-J, film start, menus,
  track/chapter controls and a clean first-run profile. Record what was tested.
- [ ] Publish the draft as a prerelease with the same tested ZIP, checksum and
  matching source tag, without rebuilding or replacing the validated artifact,
  with download links and reporting instructions in both languages.

Release readiness means the remaining checks pass and the documentation matches
the artifact users receive. A locally working EXE alone is not the release gate.

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
| Seamless transitions between playlists | Deliberately deferred; retain the known limitation until this work is resumed |
| Standard profile / installer distribution | Revisit after the portable trial; preserve the existing build option |
| BD-Live, PiP, button sounds, Java video layout and full pause synchronization | Known gaps; each needs a separate scope and validation plan |
| Full MVC/stereoscopic output and broader HDR validation | Menu/start tests do not establish these; no completion claim |
| Other video renderers or an MPC-HC port | Outside the current madVR-focused MPC-BE release scope |
| Java 25+ | No upgrade commitment while the current BD-J runtime has the SecurityManager incompatibility |

Track completed changes in the changelog. Keep this file and its Russian
translation synchronized as priorities and verified results change.
