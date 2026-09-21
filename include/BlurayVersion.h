// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Version.h"

// Keep in sync with bluray/versions.json; build-player.ps1 checks the EXE.
// Numeric upstream versions remain unchanged for resource-DLL compatibility.
#define MPCBE_BLURAY_REVISION 1
#define MPCBE_BLURAY_VERSION_STR MAKE_STR(MPC_VERSION_MAJOR) "." MAKE_STR(MPC_VERSION_MINOR) "." MAKE_STR(MPC_VERSION_PATCH) "-bluray." MAKE_STR(MPCBE_BLURAY_REVISION)
#define MPCBE_BLURAY_VERSION_WSTR _CRT_WIDE(MPCBE_BLURAY_VERSION_STR)
#define MPCBE_BLURAY_NAME_STR "MPC-BE Blu-ray"
#ifdef _WIN64
#define MPCBE_BLURAY_PRODUCT_STR MPCBE_BLURAY_NAME_STR " x64"
#else
#define MPCBE_BLURAY_PRODUCT_STR MPCBE_BLURAY_NAME_STR
#endif
