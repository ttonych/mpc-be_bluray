# Repository instructions

[Русский](AGENTS.ru.md) · [Development](docs/en/DEVELOPMENT.md) · [Roadmap](ROADMAP.md)

These instructions apply throughout this repository. Keep the Russian reference
translation in sync. Task-specific user instructions take precedence. The roadmap
records priorities; it is not an instruction to implement every listed item.

## Project scope

- This is an MPC-BE fork with HDMV and BD-J menu support, adapted for use with
  the madVR video renderer. Preserve ordinary playback and the madVR integration.
- Read `bluray/versions.json` for pinned upstream/component versions. Keep MPC-BE
  history, authors and license notices. Prefer small changes that can be merged
  with future upstream releases; avoid unrelated refactoring or mass formatting.
- Keep libbluray changes as reviewed patches and manifests in `bluray/patches`.
  Editing ignored `bluray/vendor` files alone is not a deliverable. Do not bypass
  patch/hash failures by blindly regenerating manifests or disabling checks.

## Work and build workflow

- Inspect the Git branch, status and relevant code before editing. Preserve
  unrelated local changes. Use focused `feature/`, `fix/` or `update/` branches;
  `main` is for reviewed, tested changes.
- Once the public repository is established, send locally checked changes through
  a PR to `main`, then remove the merged branch. Protect `main` from direct pushes
  and history rewriting, with passing checks required; do not require approval
  from a second person. A permanent `develop` branch is not needed.
- Experiment, compile and debug locally using `bluray/tools`. Full GitHub builds
  are manual candidate builds, not a step for every edit or push. Quick PR checks
  may run automatically. Follow existing authorization for remote actions; a
  documentation edit alone does not authorize a push or release.
- Keep upstream updates in reviewed `update/` branches; do not merge new MPC-BE
  or libbluray versions automatically. Preserve the single-repository layout:
  libbluray source pins, patches and tests stay under `bluray`.
- Release flow: local work/tests → PR → `main` → manual Actions build → draft
  Release → test its ZIP → publish that same ZIP. Record the build commit and
  checksum; never rebuild or replace the artifact between validation and publication.
  A changed artifact requires a new check. Current GitHub setup gaps are in ROADMAP.
- Use the commands in `docs/en/DEVELOPMENT.md`. Keep dependency revisions and
  download hashes pinned. Never replace a working user's player with an untested
  build; use a separate test folder.
- Default test packages keep `BlurayPortableTest=true`. Preserve the separate
  standard-profile build option for future distribution, but do not silently
  switch a portable candidate to ordinary MPC-BE profile behavior.

## Implementation and interface

- Follow the surrounding C++/MFC style. Preserve file encoding, BOM and line
  endings, especially in `.rc` resources; avoid lossy conversions.
- Put new UI text in the English and Russian resources together. Do not hardcode
  translated user-facing strings in C++ or replace native settings with a
  different UI style. Keep wording short and meaningful for ordinary users.
- Keep renderer coordinates, overlay lifetime and re-entrant callbacks correct.
  Bound-check disc-provided sizes and RLE data before allocation or copying.
- Respect disc navigation permissions. Distinguish a disc restriction or lack
  of mouse handlers from a player bug. Keep disc-specific coordinates and scripts
  in test fixtures, not runtime special cases keyed to film names.
- Settings that advertise capabilities must not be described as implementing
  absent playback features. Do not claim full stereo, HDR accuracy or all-disc
  compatibility from menu-only tests.

## User data and publication

- The portable test profile must stay beside its EXE, without falling back to
  the installed MPC-BE profile. Import must leave its source unchanged and must
  not reconnect test disc data to the original player's storage.
- Use copied or synthetic profiles for tests. Do not reset or delete real saves
  as part of automated testing. madVR and external filters can have shared
  settings; do not treat them as isolated by the player INI.
- Keep personal paths, identities, tokens, profiles, logs, crash dumps, screenshots,
  disc content, BD-J saves and local runtimes out of new commits and public ZIPs.
  Use relative paths or clear examples. Preserve upstream attribution; the
  approved generated black canvas is a build asset, not disc footage.
- Package with the explicit file selection in `bluray/tools/package-player.py`,
  never by archiving a working player directory. Java and madVR are external.
  Include licenses and the matching source/patch provenance for a release.
- Before a public push, run `python bluray/tools/check-public-tree.py` and review
  the proposed history and new author metadata. Pattern scanning is not proof
  of anonymity, and deleting a secret in a later commit does not remove it from history.

## Validation and reporting

- Run checks appropriate to the change. Documentation-only edits need link,
  wording and publication checks, not a player rebuild.
- For logic changes, run the affected component tests. Before a release candidate,
  run `bluray/tools/test-components.ps1` with a JDK 21 and build the player plus
  Russian resources. Java patch changes require the real-JAR HAVi regression.
- Reproduce relevant playback/UI changes with madVR. Use the correct disc for
  disc-specific probes. Before release, test both HDMV and BD-J from the actual
  cloud-built ZIP and verify a fresh portable profile and both UI languages.
- Report what changed, what was actually tested, and what remains unverified.
  Separate automated tests, direct observation and user-confirmed results.
  A successful compile does not establish successful playback or a cloud build.
- Update paired RU/EN docs when behavior changes, the fork changelog for visible
  changes, and both roadmaps when priorities or completion status change.

## Code Review Rules

- Flag paths that can overwrite an existing user's profile or share/reset the
  wrong disc's data, including through imported paths and shared external filters.
- Flag unsafe sizes, stale graphics pointers, callback re-entrancy problems and
  coordinate transforms that break input with scaling or black bars.
- Flag missing Russian/English resources, corrupt encodings, untracked-only
  vendor fixes, weakened integrity checks and private data in publication inputs.
- Flag unsupported claims or release status that exceeds the recorded tests.
  Focus review findings on actionable defects rather than stylistic preferences.
