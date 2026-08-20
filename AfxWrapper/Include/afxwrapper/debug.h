#pragma once
/** File containing debug macros and defines
 * 
 * Written by Patrick Zacharias for EMBU-Sys
 * Copyright (c) 2019-2024. All rights reserved.
 * 
 * SPDX-License-Identifier: MIT
 * 
 *    _____  __   __  _____  _    _        ____              
 *   |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
 *   |  __| |   V   ||  -  /| |  | |  __  \___ \ | | | |/ __|
 *   | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
 *   |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
 *                                               |___/       
 */
#include <stdio.h>

// We're currently assuming that we have colors
#ifdef NO_COLORS
#define COLOR_RED
#define COLOR_GREEN
#define COLOR_YELLOW
#define COLOR_RESET
#else
#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_RESET "\033[0;m"
#endif

#ifndef NDEBUG
// Defines for both Windows and Linux
#define STUBBED(x) printf(COLOR_RED "FIXME: Called function %s in %s:%d\n" COLOR_RESET, \
__func__, __FILE__, __LINE__)
#else
#define STUBBED(x)
#endif

#ifdef VERBOSE
#define TRACE(x, ...) printf(COLOR_YELLOW "TRACE: " x COLOR_RESET, __VA_ARGS__)
#else
#define TRACE(x, ...)
#endif

#ifdef DEBUG
#include <assert.h>
#define ASSERT assert
#define ASSERT_VALID(x) assert(x!=NULL)
#define ASSERT_KINDOF(x, y)
#else
#define ASSERT(x)
#define ASSERT_VALID(x)
#define ASSERT_KINDOF(x, y)
#endif
