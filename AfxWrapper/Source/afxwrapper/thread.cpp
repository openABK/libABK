#include "afxwrapper.h"
#include <boost/thread.hpp>

void Sleep(int milliseconds)
{
  boost::this_thread::sleep_for(boost::chrono::milliseconds(milliseconds));
}