//==--- src/Container/ThreadPool.cpp ----------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  ThreadPool.cpp
/// \brief This file implements thread pool functionality.
//
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Container/ThreadPoolException.hpp>

namespace Voxx   {
namespace Conky  {

// Returns char string pointer with an appropriate message for the type of
// the error.
const char* ThreadPoolException::message() const noexcept {
  switch (ErrorType) {
    case Type::Oversubscription : return "Pool is oversubscribed";
    default                     : return "Unknown exception type";
  }
}

}} // namespace Voxx::Conky