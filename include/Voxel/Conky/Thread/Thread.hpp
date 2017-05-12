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

/// Returns the index of the currently executing thread.
std::size_t threadIndex();

}}} // namespace Voxx::Conky::Thread