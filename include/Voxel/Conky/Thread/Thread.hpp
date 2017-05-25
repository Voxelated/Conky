//==--- Conky/Thread/Thread.hpp ---------------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  Thread.hpp
/// \brief This file defines thread related functionality.
//
//==------------------------------------------------------------------------==//

#pragma once

#include <exception>
#include <string>

namespace Voxx   {
namespace Conky  {
namespace Thread {

/// Returns the index of the currently executing thread. This needs to be set
/// appropriately by a thread on startup, or should be wrapped by an interface
/// which potentially remaps this value. This will always return a unique value.
static thread_local std::size_t threadId = 0;

}}} // namespace Voxx::Conky::Thread