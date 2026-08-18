#pragma once

#if defined(WINELIB) || defined(NO_WINDOWS)
// Function not defined in MinGW/Winelib
static int64_t Int32x32To64(int32_t left, int32_t right)
{
  return (int64_t)left * (int64_t)right;
}
#endif

#if defined(NO_WINDOWS) || defined(WINELIB)
#include <algorithm>
template <typename T>
T max(const T &left, const T &right)
{
	return std::max(left, right);
}

template <typename T>
T min(const T &left, const T &right)
{
	return std::min(left, right);
}
#endif

#ifdef NO_WINDOWS
#define _isnan std::isnan
#define _finite finite
#endif