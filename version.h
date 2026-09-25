// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT
#include <stdint.h>

#pragma once

#define FW_VERSION_MAJOR 0
#define FW_VERSION_MINOR 1
extern const uint32_t FW_BUILD_NUMBER;
    // using an extern constant here to prevent the version prebuild script 
    // from modifying an h file since this seems to invalidate the configuration
    // and force unnecessary re-compiles.
