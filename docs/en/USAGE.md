# User guide

[Русский](../ru/USAGE.md) · [Project overview](../../README.md)

## Setup and madVR

The Blu-ray menu integration is adapted for use with the **madVR video renderer** in MPC-BE.
Use a separate, writable folder for this Windows x64 test build.
Testing was performed with **madVR 210 x64**.

**If madVR is already installed, no installation or update is needed.**

**If madVR is not installed**, install it in two steps:

1. Install the [base madVR package](https://madshi.net/madVR.zip).
2. Download [madVR 210](http://madshi.net/madVRhdrMeasure210.zip) and extract the
   archive contents into that same madVR folder, confirming file replacement.

Make sure **madVR** is selected as the video renderer in MPC-BE's settings.

Updating over the base package is also described by the
[madVR developer](https://bugs.madshi.net/view.php?id=706).
The current menu graphics path requires madVR; other renderers are not supported
for menus. madVR itself and a Java runtime are not included in the player ZIP.

Open **View → Options → Playback → Blu-ray** and enable Blu-ray menus.
Open the whole disc or `BDMV\index.bdmv`. To play just a playlist without menu
navigation, open its `.mpls` file. Disc decryption tools are not included.

## Java for BD-J

HDMV menus work without Java. BD-J needs a full x64 Java runtime with AWT graphics
components. The tested runtime is **Eclipse Temurin JRE 21.0.12.1+1, Windows x64,
HotSpot**. A JDK is not required for playback.

- [Official release](https://github.com/adoptium/temurin21-binaries/releases/tag/jdk-21.0.12.1%2B1).
- [Windows x64 JRE ZIP](https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.12.1%2B1/OpenJDK21U-jre_x64_windows_hotspot_21.0.12.1_1.zip).
- [Publisher's SHA-256 file](https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.12.1%2B1/OpenJDK21U-jre_x64_windows_hotspot_21.0.12.1_1.zip.sha256.txt).

Extract Java, then select its root folder under **Blu-ray → Java and data →
Java (x64)**. That folder must contain `bin\server\jvm.dll` (or
`jre\bin\server\jvm.dll` for a runtime laid out that way). Click **Apply / OK**
and reopen the disc. Restart MPC-BE completely after replacing the Java runtime.
No system installation or `JAVA_HOME` change is needed when you set an explicit path.

An empty Java path enables automatic search when a disc opens. The settings
page checks for files; finding them does not prove that a particular JVM can
run BD-J. Keep both bundled libbluray JAR files beside the player.

Java 25 failed with `Failed initializing SecurityManager` in this project's
tests. Use the tested Java 21 runtime; a higher major version is not automatically
compatible. [Oracle documents that Security Manager is disabled from Java 24](https://docs.oracle.com/en/java/javase/24/security/security-manager-is-permanently-disabled.html).
New Java 21 updates need a playback check before becoming the recommended version.

## Controls

These are the default controls. Existing customized MPC-BE shortcuts may differ.

| Action | Control |
| --- | --- |
| Move and select in a menu | Arrow keys, Enter |
| Top menu | Alt+T; Home while handling menu navigation |
| Pop-up menu during a film | Alt+R |
| Close the pop-up menu | Escape, if the disc supports it |
| Next chapter / skip an introductory clip | Page Down or the player's Next button |
| Start of current / previous chapter | Page Up or the player's Previous button |
| Seek when the menu is closed | Left / Right |
| Volume when the menu is closed | Up / Down |

The disc may prohibit skipping or opening a menu at a particular point. MPC-BE
shows a message when skipping is forbidden. If there is no next chapter, an
allowed skip finishes the clip and returns control to the disc's navigation.
Previous goes to the current chapter's start; within its first three seconds,
it goes to the previous chapter. Chapter commands are disabled in a visible menu.

Mouse input depends on how the disc's menu was authored, particularly with BD-J.
Try the keyboard when the menu does not handle mouse input. Chapter marks are
hidden in menus/background loops and shown during the film. Chapter OSD follows
MPC-BE's seek-time display setting; a BD-J menu can also draw its own chapter OSD.

## Blu-ray settings

| Tab | Purpose |
| --- | --- |
| Disc menu | Region A/B/C, country, preferred menu/audio/subtitle languages, viewer age and 2D/3D preference |
| Java and data | Java folder, remembering disc settings/progress, storage and cache folders, Disc list |
| Compatibility | Capabilities reported to the disc menu, with descriptions and optional technical values |

Choose languages and country from the lists, or enter codes such as `eng`/`rus`
and `US`/`RU`. A disc can keep using its saved language choice. Region is the
player region reported to the disc. Compatibility settings describe capabilities;
they do not install decoders, configure madVR or enable unimplemented PiP/3D features.

In Compatibility, double-click a value or press **Enter / F2** to edit it.
Enter accepts the edit; Escape cancels it. Search is case-insensitive. Resetting
a parameter removes its manual override. Numeric masks accept decimal or `0x…`.
Changes saved with **Apply / OK** take effect when the disc is next opened.
Changing a data folder does not move existing files automatically.

## Disc settings and progress

Open **Java and data → Disc list…** to find, rename or reset a saved disc,
or open its data folder. “Last opened” means the time the disc was opened, not
the time its Java application last saved progress. Saved data is managed by the
disc application, so not every disc offers resume playback.

By default, data lives in `bdj-data` beside `mpc-be64.exe`:

| Folder | Contents |
| --- | --- |
| `catalog` | Disc names, custom labels and opening history |
| `persistent` | Shared BD-J persistent data |
| `cache` | Temporary data from disc applications |
| `discs/<id>/<generation>/persistent` | Separate storage selected after a disc reset |

Inside persistent storage, organization and Java application IDs group files.
They are namespaces, not necessarily one folder per film: discs can share data.

**Reset data…** asks for confirmation and immediately selects empty storage for
that disc's next opening. Old files are retained; other discs and the current
playback session are unaffected. Cancelling the main settings dialog does not
undo this reset. Reset is unavailable for a shared legacy entry without a
reliable disc ID. A custom storage root uses `mpc-be-discs/<id>/<generation>`;
the catalog and reset marker remain beside the player.

## Existing settings and updates

The experimental portable build always uses `mpc-be64.ini` beside the EXE. It
does not fall back to the installed MPC-BE registry profile if the INI is missing.
The first-run window offers four choices:

| Source | What the player checks |
| --- | --- |
| Registry | Current user's `HKCU\Software\MPC-BE` |
| Detected INI | `%APPDATA%\MPC-BE\mpc-be64.ini`, then `mpc-be.ini` |
| Select an INI manually | The settings file from another portable MPC-BE folder |
| Default settings | A new profile without importing another player's preferences |

Registry and detected INI availability are shown independently, with their
locations. Close the source player before importing. Import copies ordinary
settings and shader files, not history, playlists or BD-J saves. It resets the
imported BD-J storage paths to keep test data separate. The source profile is
unchanged. An existing test profile is kept without repeating the import.

History, the web server, WinLIRC and global media keys start disabled. File
associations and Explorer integration are unavailable in this test mode. If the
folder is not writable, startup stops instead of using a shared profile.

External filters must remain available. Relative filter DLL paths are resolved
against the source player's folder during import, so keep that folder if it
contains filters. madVR and other filters keep their own potentially shared
settings; changing those component settings can affect other players.

The player checks releases of **MPC-BE Blu-ray**, including published prereleases,
not releases of the original MPC-BE. It opens the release page; it does not install
or replace files automatically.

For an update, close the test player and back up its INI and disc data, including
any custom storage folder. Extract the new package separately and replace only
the program files after checking the release instructions. Do not overwrite
your existing INI with the clean INI from a new ZIP.

## Troubleshooting and feedback

| Symptom | First checks |
| --- | --- |
| Video plays without menu graphics | Select madVR, enable Blu-ray menus, and open the disc/index rather than a playlist |
| BD-J fails to start | Check x64 Java 21, its root folder and both libbluray JARs; restart after changing Java |
| Mouse does nothing in a menu | Try arrows and Enter; some discs only implement keyboard navigation |
| Introductory clip cannot be skipped | The disc may forbid it; check the OSD message |
| CRC or disc read error | Verify the mounted disc is readable outside MPC-BE, remount if needed, and reopen it |
| Old language or resume prompt | Check Disc list and, if desired, reset that disc's data |
| Pause between playlists | This remains a known limit of the current playback integration |

See the [current limits](../../README.md#current-limits) before reporting a problem.
Include the fork version, Windows version, madVR and Java versions, disc edition
and region, and steps with expected/actual results. State whether input was by
mouse or keyboard. Share only the relevant, reviewed log excerpt; remove personal
paths. Do not upload a whole player folder, personal INI, disc saves or disc files.
