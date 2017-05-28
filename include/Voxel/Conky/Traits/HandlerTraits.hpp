//==--- Conky/Traits/HandlerTraits.hpp --------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  HandlerTraits.hpp
/// \brief This file provides traits for handler classes used by the thread pool
///        to handle the popping and pushing of elements onto the queue.
//
//==------------------------------------------------------------------------==//

#pragma once

namespace Voxx   {
namespace Conky  {
namespace Traits {

/// The HandlerTraits struct defines the traits for the handler used to create
/// a custom ThreadPool.
/// \tparam Object  The type of the object the handler handles.
template <typename Object>
struct HandlerTraits {
  /// Alias to define the type of the object handled by the handler.
  using Type = Object;
};

}}} // namespace Voxx::Conky