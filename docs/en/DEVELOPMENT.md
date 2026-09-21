# Development and maintenance

[Русский](../ru/DEVELOPMENT.md) · [Project overview](../../README.md)

Repository rules: [AGENTS.md](../../AGENTS.md). Priorities and pending release
checks: [ROADMAP.md](../../ROADMAP.md).

## Layout and versions

MPC-BE source and its upstream Git history stay at the repository root.
The `bluray` folder holds integration build tools, probes, local patches and
[pinned versions](../../bluray/versions.json). Downloaded libbluray source and
build dependencies are ignored, generated files. There is no separate libbluray
fork repository at this stage.
Reconsider a separate library repository only if the maintained changes need
to serve another player, such as a future MPC-HC integration.

The current base is upstream **1.9.1**, commit
`d2c7b28a22ffe5ddd72632c9076d594ac6d2178f`. The first fork version is planned as
`1.9.1-bluray.1`; another release on that base becomes `1.9.1-bluray.2`, and a
release on a new base starts at, for example, `1.9.2-bluray.1`.
The local player's product version still reads `1.9.1`; the package name and
manifest identify the fork revision. The final in-app version label is pending.
The repository name is `mpc-be_bluray`; the intended display name is **MPC-BE
Blu-ray**, identified as an unofficial modification. The former `1.9.1 dev` base
has been replaced by the official release; the planned version has no `dev` suffix.

Use `main` for reviewed, tested code, with short `feature/`, `fix/` and `update/`
branches. A permanent develop branch is not needed. The source repository is
public; the first downloadable release is still being prepared.

## Branches and release flow

Work locally on the appropriate short branch, run relevant tests, then send the
prepared changes as a PR. Review the diff and passing checks before merging into
`main`; delete the merged branch afterward. Branch protection requires PRs and
a passing `source-check`, including for administrators. Force pushes and branch
deletion are disabled. A second person's approval is not required.

The release sequence is:

1. Develop and test locally, review the PR, then merge into `main`.
2. Run the full Actions build manually for the chosen commit in `main`, using
   the same build scripts as local development.
3. Create a draft Release for that exact commit, with the resulting ZIP, SHA-256,
   component versions and English/Russian release notes. Leave the Release
   unpublished until validation is complete.
4. Test that cloud-built ZIP with madVR, HDMV, BD-J and a clean portable profile.
5. Publish the same ZIP and checksum, with its matching source tag. Do not rebuild
   or substitute files after testing; any change requires another candidate check.

The current workflow only uploads Actions artifacts. Draft creation and publication
are separate release steps and are not automated by the build workflow.
Download and preserve the ZIP and its checksum before the artifacts' 14-day
retention expires. Public releases use cloud-built binaries; local packages
remain for development and diagnosis.

## Local build

Prerequisites:

- Windows x64, Git, PowerShell 7 (`pwsh`), Python 3.13 and `7z` on PATH.
- Visual Studio 2022 with Desktop development with C++, MSVC v143, ATL/MFC and
  Windows SDK `10.0.19041.0` (or explicitly pass an installed SDK to the build script).
- JDK 21 containing `bin/javac.exe`, for compiling libbluray's Java classes.
  Playback only needs the JRE described in the [user guide](USAGE.md#java-for-bd-j).

Clone this fork using its GitHub clone URL, then run these commands from its root.
Set `$jdkRoot` to your JDK folder; the example path is only a placeholder.

```powershell
git submodule update --init --recursive
git config core.hooksPath .githooks
$jdkRoot = 'C:\Java\jdk-21'
./bluray/tools/prepare-dependencies.ps1
./bluray/tools/build-libbluray.ps1 -DependencyRoot "$PWD/bluray/build/vcpkg_installed/x64-windows"
./bluray/tools/build-bdj.ps1 -JavaHome $jdkRoot
./bluray/tools/test-components.ps1 -JavaHome $jdkRoot
./bluray/tools/build-player.ps1
```

Dependency preparation downloads the pinned MPC-BE GCC/MSYS toolchain and builds
the vcpkg manifest at its pinned baseline. Existing vcpkg checkouts with another
revision are rejected. The libbluray and Ant source archives have checksum checks.
Python build packages are pinned in
[requirements-build.txt](../../bluray/tools/requirements-build.txt).
See the retained [upstream build instructions](../Compilation.txt) for context;
the commands above are the entry point for this fork.

`build-player.ps1` supports `-SdkVersion`, `-ToolchainRoot` and `-RuntimeDirectory`.
An existing toolchain can also be supplied through `MPCBE_MSYS`. All paths are
local configuration; do not commit a machine-specific environment file.

| Output | Location |
| --- | --- |
| libbluray DLL, JARs and probes | `bluray/out/libbluray-1.5.0-x64` |
| Player and Russian resource DLL | `bluray/out/mpc-be-bluray-x64` |
| Player build log | `bluray/diagnostics/logs/player-build.log` |
| Player intermediate files / symbols | `_bin` |
| Test results and fixtures | `bluray/build` and `bluray/diagnostics` |

Existing player INI files are retained by the build. Default builds set
`BlurayPortableTest=true`: an isolated INI profile with first-run import.
`build-player.ps1 -StandardProfile` uses the ordinary MPC-BE profile policy for
future standard distribution; the portable test packager rejects that mode.
It is not the mode used for the initial public test package.

## Checks and packaging

`test-components.ps1` runs the synthetic C++ checks, both patch regression suites,
HAVi tests against the built JAR, and publication-checker tests. It needs no disc.
Disc-specific probes include `test-mouse-submenus.ps1 -Disc 'V:\'` for Baby Boom
and `test-menu-background.ps1 -KingdomDisc 'V:\'` for The Kingdom. Use the matching
disc; these are not generic disc validators. `test-hdmv.ps1 -Disc 'V:\'` exercises
HDMV navigation. Logs and generated fixtures remain local.

Playback checks must use **madVR**, the renderer this integration was adapted for.
Check an HDMV and a BD-J disc: introduction/stills, menu selection, film start,
track selection, top/pop-up menus, chapter keys, window scaling and disc changes.
Include a fresh portable profile and both interface languages when changing UI
or import code. Component tests alone cannot verify visible video, audio or HDR.

For a public candidate, review and commit the source, rebuild, then package:

```powershell
python bluray/tools/check-public-tree.py
python bluray/tools/package-player.py
```

For local experiments only, `package-player.py --allow-dirty` accepts a build
marked with uncommitted changes and adds `-local` to its name. Packages go to
`bluray/out/packages`; existing archives are not overwritten. The packager checks
program hashes, selects an explicit program file set and creates a clean INI.
Java, madVR, personal profiles, disc data, logs and debugging symbols are excluded.

The current candidate packager does not yet include these new user documents.
Connecting the final bilingual instructions to the ZIP is required before the
first public release. Do not distribute the existing local candidate as a finished release.

## GitHub Actions

- [Quick checks](../../.github/workflows/bluray-checks.yml) run on pull requests
  or manually. They check publication inputs and the checker itself, authored
  Markdown links/examples, and the fork's RU/EN resource IDs, format arguments
  and dialog controls. Translation quality and visual layout still need review.
  Run `python bluray/tools/check-docs-and-resources.py` for the same documentation
  and resource checks locally.
- [Full build](../../.github/workflows/bluray-build.yml) runs only when manually
  requested, on `windows-2022`. It builds native libbluray, Java components,
  component tests, MPC-BE and Russian resources, then uploads a ZIP and checksum.
- Ordinary pushes do not start a full build. Develop and test locally, then
  request a release candidate after the reviewed changes are merged into `main`.
- The workflow has read-only repository permissions and does not publish a
  GitHub Release. Artifacts are retained for 14 days.

Actions are pinned by commit. Dependency downloads, the vcpkg baseline and build
versions are pinned separately. This does not claim bit-for-bit reproducibility.
The JDK bootstrap downloads the exact Temurin 21.0.12.1+1 archive and checks its
SHA-256; `setup-java` cannot parse this four-part version. Local builds pass;
the first successful cloud build is still pending. Its ZIP must be tested
separately with madVR.

## Updating MPC-BE

Keep `origin` for this fork and `upstream` for the original MPC-BE repository.
Preserve upstream history and merge a selected official release tag into an
`update/` branch. Do not replace the repository with a snapshot of upstream files.
For example, use `update/mpc-be-1.9.2` when adopting that release. Do not auto-merge
upstream releases or advance the base without local playback validation.
Update submodules to that tag's recorded revisions, resolve integration changes,
and update `bluray/versions.json` and both language changelogs.

Check the native version resources, package name and `update_revision.cmd`.
The revision script intentionally selects numeric upstream tags, excluding
hyphenated fork tags, so a fork tag does not break upstream revision parsing.
Run local tests and playback checks, review the PR, then merge the tested change
to `main`. Build and test the cloud candidate before tagging a public release.

## Updating libbluray

Use a separate `update/libbluray-<version>` branch and a reviewed PR, following
the same local-first process as MPC-BE updates.

The [two local patches](../../bluray/patches/README.en.md) have base/result hashes
and regression tests. Neither has been submitted to or accepted by upstream.
Before changing libbluray, check whether upstream has fixed the original cause.
Remove a patch only after its regression test passes without it, or port it
deliberately to the new source and review the changed behavior.

Update the source archive/hash, patch manifests, vendored API headers and versioned
paths/names in build scripts, the package allowlist and Java integration. Use
`rg '1\.5\.0' bluray include/libbluray src/apps/mplayerc` to locate version-specific
references. Rebuild DLLs and both JARs together, run patch/HAVi/component checks,
then repeat HDMV and BD-J playback checks. Refresh licenses and dependency pins
when required. Never use patch `--export` merely to silence an unexpected hash error.

## Publication and personal data

The pre-push hook runs `check-public-tree.py` when enabled by the local Git config
above. The checker scans new working files and introduced Git blobs, including
content removed by later commits. It detects common secrets, local paths and
unwanted artifacts; it cannot prove that all text is anonymous.

Review new commits and their author name/email before pushing. Use the intended
public identity, for example a GitHub noreply address. Keep profiles, logs,
screenshots, disc files, Java runtimes, private saves and machine paths out of
commits. Preserve upstream author attribution and license notices.

For the first release, finish the version label and packaged documentation,
check the full proposed history, run the cloud build and test its ZIP. Publish
the tested package with its checksum and matching source tag as a prerelease.
Keep English and Russian instructions/changelogs synchronized.
