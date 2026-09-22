# Local libbluray 1.5.0 patches

[Русский](README.md) · [Project maintenance](../../docs/en/DEVELOPMENT.md)

Run the commands below from the `mpc-be_bluray` repository root. These patches
have not been submitted to or accepted by upstream VideoLAN. They are maintained
changes for this integration, whose menu output is adapted for madVR.
Before the first check, complete the [full local build](../../docs/en/DEVELOPMENT.md#local-build):
rebuilding the player below requires both the native DLL and the two JARs.

## Navigation: playmark-seek-v1 (2026-09-22)

After a seek, the original tracker skipped marks at exactly the next packet to
read. Several marks can share that packet because they map to the same random
access unit. A BD-J menu waiting for one of those marks could remain on its
background video after a return from the film.

The patch includes the seek position when finding the next mark. Events still
fire only after reading past that packet, once per mark until another seek.
It changes native `bluray.c`; it contains no disc-specific conditions and does
not change the JARs or rendering. On the tested UHD BD-J disc, the user confirmed
that changing the menu language and returning from the film worked. Five
recorded returns delivered both marks and produced visible graphics again.

The native build applies `mouse-page-v1` followed by `playmark-seek-v1` because
both touch `bluray.c`. It accepts only a complete pristine, intermediate, or
fully patched state, with exact hashes; unexpected edits stop the build.

```powershell
python bluray/tools/libbluray-local-patch.py --component native
python bluray/tools/test-libbluray-local-patch.py --component native
python bluray/tools/test-playmark-seek.py
```

The synthetic regression compiles the actual tracking functions and covers equal
positions, multiple marks per access unit, one-time delivery, backward seeks,
skipped marks, empty lists and large byte offsets. The original code fails it.
The same mark loss and corrected delivery were reproduced with the disc in a
separate native probe; this does not establish full disc compatibility.

For deliberate maintenance, export this component with `--component playmark-seek
--export --base-source <source-with-mouse-page-v1>`. That base must match the
mouse patch's hashes. Maintain the mouse patch in a separate source copy before
applying the mark patch; never fold the two patches into one by exporting the
combined tree as `mouse-page`.

## BD-J: bdj-toggle-v1 (2026-09-20)

`libbluray-1.5.0-bdj-toggle.patch` fixes HAVi button activation. In the original
1.5.0 code, Enter called `HActionableHelper` directly, bypassing the component's
virtual `processHActionEvent`. `HToggleButton` also lacked state toggling on an
action. On Double Jeopardy (Remastered), Settings opened while its button state
remained off. When focus returned to Play, the disc checked that state and left
the panel open, allowing Settings and Scenes to overlap.

Enter now dispatches the event through the component. `HToggleButton` changes
state before notifying listeners and respects its group and forced selection.
Disabled buttons do not activate; deselection uses the unset sound. Ordinary
graphic/text buttons still receive one notification per Enter. The patch has
no disc names, coordinates or disc-specific Xlet classes. It changes two Java
files; this fix does not change the native DLL or MPC-BE.

`bluray/tools/build-bdj.ps1` verifies/applies the patch before building the JARs.
The JSON manifest records base/result file hashes and the patch hash. To check
and rebuild the existing patch:

```powershell
python bluray/tools/test-libbluray-local-patch.py --component bdj-toggle
./bluray/tools/build-bdj.ps1 -JavaHome 'C:\Java\jdk-21'
./bluray/tools/test-bdj-toggle.ps1 -JavaHome 'C:\Java\jdk-21'
./bluray/tools/build-player.ps1
```

Only when deliberately changing the Java patch, after reviewing the source edits,
export it:

```powershell
python bluray/tools/libbluray-local-patch.py --component bdj-toggle --export
```

This command overwrites the patch and its manifest. Review the resulting diff,
then repeat the checks and builds above. A normal build does not need an export;
never use it to bypass an unexpected hash error.

The test uses the actual HAVi classes from the built JAR. Only the JNI logger
and Xlet-context look-and-feel lookup are stubbed to run without a disc/window.
It checks virtual dispatch, listener/state ordering, repeated input, release,
sound events, disabled buttons and groups. The unpatched JAR fails; the patched
one passes. Playback regression: Settings → subtitle selection → Play, then
Scenes → Down → main menu.

Sound-event tests validate the HAVi logic; playback of menu button sounds is
not implemented by the player integration. Keep that distinction when reporting results.

On an upstream update, check whether the cause has been fixed, then port or
remove the patch and repeat the tests. The modified source retains libbluray's
LGPL notices.

## HDMV: mouse-page-v1 (2026-09-19)

On Baby Boom, the bottom navigation is drawn into the video. The chapter page
contains an invisible auto-action button whose only command is `SetButtonPage`
back to the main navigation. Down can reach it, but `bd_mouse_select` tests only
buttons with selected graphics on the current page. A click on the bottom
navigation from the submenu therefore returns no selection.

The patch adds `bd_mouse_select_page` and an internal controller message, keeping
existing API behavior. MPC-BE uses this extension only for a click that did not
hit an active button. It considers enabled invisible auto-action buttons with a
single unconditional `SetButtonPage` command and immediate operands. An enabled
selectable button must reference the transition, and a selectable target-page
button must be under the pointer. Ambiguous transitions, effects, input masks
and complex scripts are rejected.

The command runs through the normal HDMV VM with `bd_read_ext(..., len=0)`.
The player repeats the hit test before activating the selected item. Baby Boom
coordinates appear only in the test script; the player and DLL read geometry
and commands from the disc.

This is limited compatibility with a particular menu authoring pattern, not a
general solution for all discs or BD-J. Hovering over bottom navigation does not
change the page; a click performs the transition.

`bluray/tools/build-libbluray.ps1` checks the VideoLAN archive hash and calls
`bluray/tools/libbluray-local-patch.py`. The patch manifest holds before/after
SHA-256 hashes for four source files. Reapplying is idempotent; unexpected source
changes or another source version stop the build. `--export` is for intentional
patch maintenance after editing, not for bypassing a failed integrity check.

To repeat the checks after a full local build, mount Baby Boom at `V:` for the
disc probes below, or pass its actual path to `-Disc`:

```powershell
python bluray/tools/test-libbluray-local-patch.py
./bluray/tools/build-libbluray.ps1 -DependencyRoot "$PWD/bluray/build/vcpkg_installed/x64-windows"
./bluray/tools/test-mouse-submenus.ps1 -Disc 'V:\'
./bluray/tools/test-hdmv.ps1 -Disc 'V:\'
./bluray/tools/test-menu-coordinates.ps1
./bluray/tools/build-player.ps1
```

The submenu probe checks graphical results for chapters, SET UP, SPECIAL
FEATURES, empty areas, both page arrows and starting the film only through PLAY
MOVIE. This is a libbluray component check; real pointer input and madVR output
require separate playback checks.

For a new upstream version, compare the affected functions, port the patch,
update pinned versions/hashes, rebuild and repeat component/playback checks.
The build does not silently apply shifted patches or promise compatibility with
future libraries. Before proposing upstream adoption, discuss the fallback
semantics and test other discs. Preserve LGPL notices and distribute the patch
with the modified library.
