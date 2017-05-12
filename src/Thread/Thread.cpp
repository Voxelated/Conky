//==--- src/Thread/Thread.cpp ------------------------------ -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  Thread.cpp
/// \brief This file implements thread functionality.
//
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Thread/Thread.hpp>
#include <Voxel/SystemInfo/CpuInfo.hpp>
#include <Voxel/Utility/Debug.hpp>


#if defined(__APPLE__)
# include <sys/sysctl.h>
#elif defined(__linux__)
# include <sys/sysctl.h>
# include <sched.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace Voxx   {
namespace Conky  {
namespace Thread {

}}} // namespace Voxx::Conky::Thread