#pragma once

#include <iostream>

#include <boost/filesystem.hpp>

/*
 * Debugging output -- should be used for any information that helps in
 * debugging (i.e., it's ok if it slows things down).
 */

#define FILE boost::filesystem::path(__FILE__).filename().c_str()

#ifdef DEBUG_OUTPUT
#define DEBUG_LOCATION "(" << FILE << ":" << __LINE__ << ")"
#define DMSG(msg) std::cerr << DEBUG_LOCATION << "  " << msg << std::endl;
#define DOUT(x) std::cerr << DEBUG_LOCATION << "  " << x;
#else
#define DMSG(msg) (void)0
#define DOUT(x) (void)0
#endif