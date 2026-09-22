# mpc-be_bluray

[Русский](README.ru.md)

**Blu-ray menu support in MPC-BE, adapted for use with the madVR video renderer.**
The project adds HDMV and BD-J menu support through libbluray.
This lets you use Blu-ray menus and watch films in MPC-BE with madVR as the
video renderer.

Testing was performed with **madVR 210 x64**.

This is an independent, experimental fork. Its current base is the official
**MPC-BE 1.9.1 release**, with **libbluray 1.5.0** and maintained local patches.
Fork version: **1.9.1-bluray.1**.

This fork’s modifications were developed with the help of ChatGPT Astra.
The project maintainer defines the functionality and interface requirements
and tests the player with real Blu-ray discs.

## Download and quick start

Download [1.9.1-bluray.1 — Windows x64 ZIP](https://github.com/ttonych/mpc-be_bluray/releases/download/1.9.1-bluray.1/mpc-be_bluray-1.9.1-bluray.1-x64.zip).
This is a preliminary release; see its [validation notes](https://github.com/ttonych/mpc-be_bluray/releases/tag/1.9.1-bluray.1).
The portable package includes English and Russian interfaces. Java and madVR
are installed separately.
Open `Readme.html` or `Readme.ru.html` in the extracted ZIP for offline instructions.

1. Extract the candidate ZIP into a separate, writable folder.
2. Start `mpc-be64.exe`. Choose whether to copy settings from your existing
   MPC-BE or start with defaults. Close the source player before copying settings.
3. **If madVR is already installed, no installation or update is needed.**
   If it is not installed, install the [base package](https://madshi.net/madVR.zip),
   then extract [update 210](http://madshi.net/madVRhdrMeasure210.zip) over it
   in the same madVR folder, replacing the existing files.
   Make sure **madVR** is selected as the video renderer in MPC-BE's settings.
   See the [installation steps](docs/en/USAGE.md#setup-and-madvr).
   The menu graphics implementation uses madVR; menu support with other
   renderers is not currently provided.
4. For BD-J, install the tested **Eclipse Temurin JRE 21.0.12.1+1, Windows x64,
   HotSpot**, and select its folder under **Blu-ray → Java and data**.
   See the [Java setup instructions and official download](docs/en/USAGE.md#java-for-bd-j).
   HDMV menus do not need Java.
5. Open the whole disc or its `BDMV\index.bdmv` file. Opening an individual
   `.mpls` plays that playlist without the disc's menu navigation.

Use the arrow keys and **Enter** in menus; **Alt+T** opens the top menu and
**Alt+R** requests the pop-up menu. Some discs also support mouse control.
Command availability and mouse support depend on the disc's menu implementation.

## What this fork adds

- HDMV and BD-J navigation, with menu graphics displayed through madVR.
- Keyboard navigation and mouse input where the disc supports it.
- Still images, menu loading graphics, audio/subtitle selection through disc
  menus, and chapter navigation integrated with MPC-BE.
- Blu-ray settings for region, languages, Java, disc data and compatibility.
- A readable list of saved discs, with renaming and per-disc data reset.
- A separate portable test profile, with optional import from an existing MPC-BE.

## Existing MPC-BE installations

Keep this test build in its own folder. It uses its own `mpc-be64.ini`, including
when that file is initially missing. Import reads the selected source profile;
it does not change it. Disc saves and playback history are not imported.

madVR and other external filters have their own settings, which can be shared
between players. Profile isolation does not create a separate madVR installation.
See [settings import and updates](docs/en/USAGE.md#existing-settings-and-updates).

## Current limits

Development and playback checks focus on **MPC-BE + madVR**. This is not a promise
of compatibility with every disc or every madVR version.

- Transitions between different playlists can pause while the playback graph is rebuilt.
- BD-Live, picture-in-picture playback and menu button sounds are not implemented.
- Some BD-J menus only support keyboard input. Java-controlled video layout and
  complete pause synchronization with Java are not implemented.
- Tests on 3D discs cover menus and starting playback; full MVC/stereoscopic
  output has not been validated. HDR menu fixes do not certify HDR video accuracy.
- Disc decryption tools are not included; the disc contents must already be readable.

The local build on the official 1.9.1 base has been checked with **Baby Boom
(HDMV)** and **Ford v Ferrari (BD-J)**. Each cloud candidate needs a separate
playback check; see its release notes for results specific to the downloadable ZIP.

## Documentation

- [User guide](docs/en/USAGE.md): setup, controls, settings, disc data and troubleshooting.
- [Development](docs/en/DEVELOPMENT.md): local builds, tests, GitHub Actions and upstream updates.
- [Fork changelog](docs/en/CHANGELOG.md).
- [Roadmap](ROADMAP.md) and [repository instructions](AGENTS.md).
- [Local libbluray patches](bluray/patches/README.en.md).

## Upstream and acknowledgements

This fork builds on [MPC-BE](https://github.com/Aleksoid1978/MPC-BE) and
[VideoLAN libbluray](https://www.videolan.org/developers/libbluray.html).
madVR is a separate third-party component and is not redistributed here.

The original [MPC-BE documentation](docs/README.md),
[author credits](docs/Authors.txt), [MPC-HC credits](docs/Authors%20mpc-hc%20team.txt)
and [license](LICENSE.txt) are retained. Component license texts accompany the
binary package. The local libbluray patches are not upstream releases or
upstream-accepted fixes.
