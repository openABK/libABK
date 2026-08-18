#pragma once
/** File exposing the TCHAR defines
 * 
 * This file selects the appropriate string functions to be used with TCHAR.
 * 
 * Written by Patrick Zacharias for EMBU-Sys
 * Copyright (c) 2019-2024. All rights reserved.
 * 
 * SPDX-License-Identifier: MIT
 * 
 *    _____  __   __  _____  _    _        ____              
 *   |  ___||  \ /  ||  _  \| |  | |      / ___|  _   _  ___ 
 *   |  __| |   ´   ||  -  /| |  | |  __  \___ \ | | | |/ __|
 *   | |___ | |\_/| ||  _  \| \__/ | |__|  ___) || |_| |\__ \
 *   |_____||_|   |_||_____/ \___ /       |____/  \__  ||___/
 *                                               |___/       
 */
#include <stdio.h>

#ifndef UNICODE
#define ASCII_CHAR
#endif

#ifdef ASCII_CHAR
typedef char TCHAR;
#else
typedef wchar_t TCHAR;
#endif

size_t _tcslen(const TCHAR *str);
int _tcscmp(const TCHAR *left, const TCHAR *right);
int _tcsicmp(const TCHAR *left, const TCHAR *right);
int _tcsncmp(const TCHAR *str1, const TCHAR *str2, size_t max);
int _tcsnicmp(const TCHAR *str1, const TCHAR *str2, size_t max);

const TCHAR * _tcsstr (const TCHAR *str, const TCHAR *strToFind);
TCHAR *_tcscpy(TCHAR * dest, const TCHAR *src);
size_t _tcscpy_s(TCHAR *pszDest, size_t szBytes, const TCHAR *pszSource);

TCHAR *_tcsncpy(TCHAR *dest, const TCHAR *src, size_t max);
TCHAR *_tcsnccpy(TCHAR *dest, const TCHAR *src, size_t max);
size_t _tcsncpy_s(TCHAR *dest, size_t dest_count, const TCHAR *src, size_t src_count); //strncpy(dest, src, dest_count /*- 1*/)
TCHAR *_tcscat(TCHAR *dest, const TCHAR *src);

const TCHAR *_tcschr (const TCHAR *str, int charToFind);
TCHAR *_tcschr (TCHAR *str, int charToFind);

const TCHAR *_tcsrchr (const TCHAR *str, int charToFind);
TCHAR *_tcsrchr (TCHAR *str, int charToFind);

int _stscanf (const TCHAR *source, const TCHAR *format, ...);

int _stprintf_s(TCHAR *dest, size_t dest_count, const TCHAR *format, ...);

FILE *_tfopen(const TCHAR *__filename, const TCHAR *__modes);

// TCHAR macro
#ifndef _T
#ifdef ASCII_CHAR
#define _T(x) x
#else
#define _T(x) L ## x
#endif
#endif
