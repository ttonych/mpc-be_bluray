// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <PortableTestConfig.h>

// Called before normal settings loading. A cancelled/failed setup must not save.
bool InitializePortableTest();
