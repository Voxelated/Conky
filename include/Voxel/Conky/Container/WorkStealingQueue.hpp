//==--- Conky/Container/WorkStealingQueue.hpp -------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  WorkStealingQueue.hpp
/// \brief This file provides the definition of a lock-free work stealing queue.
//
//==------------------------------------------------------------------------==//

#pragma once

#include <Voxel/Math/Math.hpp>
#include <array>
#include <atomic>
#include <optional>

namespace Voxx {
namespace Conky {

/// The WorkStealingQueue is a concurrent lock-free work stealing queue which
/// can hold a fixed size number of tasks per thread.
/// 
/// This imlementation is base on the original paper by Chase and Lev:
/// 
///   [Dynamic circular work-stealing deque](
///     http://dl.acm.org/citation.cfm?id=1073974)
///     
/// __Note:__ When using this class for per thread queues, ensure that the
///           instances are aligned so that there is no false sharing.
/// 
/// \tparam Object      The type of the data in the queue.
/// \tparam MaxObjects  The maximum number of objects for the queue.
template <typename Object, uint32_t MaxObjects>
class WorkStealingQueue {
 public:
  ///==--- Aliases ---------------------------------------------------------==//
  
  /// Defines the type used for the queue top and bottom indices.
  using SizeType      = uint32_t;
  /// Defines the atomic type used for the index pointers.
  using AtomicType    = std::atomic<SizeType>;
  /// Defines the type of the objects in the queue.
  using ObjectType    = Object;
  /// Defines an optional type to return in the case that the queue is empty.
  /// Despite optional using more space than required, it is only used to return
  /// values from pop and steal, so the overhead should be acceptable. If not,
  /// another approach can be looked at. 
  using OptionalType  = std::optional<Object>;
  /// Defines the type of container used to store the queue's objects.
  using ContainerType = std::array<ObjectType, MaxObjects>;

  //==--- Con/destruction --------------------------------------------------==//
  
  /// Constructor -- ensures that the queue is a power of 2 size, as we need the
  /// performance optimi
  
  //==--- Methods ----------------------------------------------------------==//
  
  /// Pushes an object onto the front of the queue, when the object is an
  /// rvalue reference type. This does not check if the queue is full, and hence
  /// it __will overwrite__ the least recently added element if the queue is
  /// full. 
  /// \param[in] object   The object to push onto the queue.
  void push(ObjectType&& object) {
    auto bottom     = wrappedIndex(Bottom.load(std::memory_order_relaxed));
    Objects[bottom] = std::move(object);

    // Ensure that the compiler does not reorder this instruction and the
    // setting of the object above, otherwise the object seems added (since the
    // bottom index has moved) but isn't (since it would not have been added).
    Bottom.store(bottom + 1, std::memory_order_release);
  }

  /// Pushes an object onto the front of the queue, when the object is a
  /// const lvalue reference to the object. This does not check if the queue
  /// is full, and hence it __will overwrite__ the least recently added element
  /// if the queue is full. 
  /// \param[in]  object  The object to push onto the queue.
  void push(const ObjectType& object) {
    auto bottom     = wrappedIndex(Bottom.load(std::memory_order_relaxed));
    Objects[bottom] = object;

    // Ensure that the compiler does not reorder this instruction and the
    // setting of the object above, otherwise the object seems added (since the
    // bottom index has moved) but isn't (since it would not have been added)
    Bottom.store(bottom + 1, std::memory_order_release); 
  }
  
  /// Pops an object from the front of the queue. This returns an optional
  /// type which holds either the object at the bottom of the queue (the most
  /// recently added object) or an invalid optional. We copy the object into the
  /// optional as it does not make sense to do anything else. Other options
  /// would be:
  /// 
  /// 1. Return a reference or pointer: This is just too error prone, since a
  ///    subsequent call to push would then overwrite the value being referenced
  ///    , which is obviously not an option.
  ///   
  /// 2. Move returned object into the result: This isn't a valid option since
  ///    the memory needs to be reused to store subsequent objects added to the
  ///    queue.
  ///    
  /// Thus the usage is:
  /// 
  /// ~~~cpp
  /// // If using the object directly:
  /// if (auto object = queue.pop())
  ///   object->invokeObjectMemberFunction();
  ///   
  /// // If using the object for a call:
  /// if (auto object = queue.pop())
  ///   // Passes object by reference:
  ///   functionUsingObject(*object);
  /// ~~~
  /// 
  /// Returns an optional type which is in a valid state if the queue is not
  /// empty.
  OptionalType pop() {
    auto bottom = Bottom - 1;
    Bottom.store(bottom, std::memory_order_relaxed);

    // This acts as a barrier to ensure that top is always set __after__ bottom,
    // and that neither the compiler nor the cpu reorder the instructions.
    
    // All non-relaxed atomic operations cause ordering within their own thread,
    // so this acquire operation is a barrier between it and the store above.
    auto top = Top.load(std::memory_order_acquire);
    if (top > bottom) {
      Bottom.store(top, std::memory_order_relaxed);
      return OptionalType{};
    }

    auto object = make_optional(Objects[wrappedIndex(bottom)]);
    if (top != bottom)
      return object;

    // If we are here there is only one element left in the queue, and there may
    // be a race between this method and steal() to get it. If this exchange is
    // true then this method won the race (or was not in one) and then the
    // object can be returned, otherwise the queue has already been emptied.
    bool exchange = Top.compare_exchange_strong(top + 1                  ,
                                                top                      ,
                                                std::memory_order_release);
    return exchange ? object : OptionalType{};
  }

  /// Steals an object from the top of the queue. This returns an optional
  /// type which holds either the object at the top of the queue (the least
  /// recently added object) or an invalid optional. We copy the object into the
  /// optional as it does not make sense to do anything else. Other options
  /// would be:
  /// 
  /// 1. Return a reference or pointer: This is just too error prone, since if
  ///    the queue is almost full, and objects are being pushed very quickly,
  ///    then the referenced object would be overwritten by the thread pushing
  ///    to the queue.
  ///   
  /// 2. Move returned object into the result: This isn't a valid option since
  ///    the memory needs to be reused to store subsequent objects added to the
  ///    queue.
  ///
  /// Example usage is:
  /// 
  /// ~~~cpp
  /// // If using the object directly:
  /// if (auto object = queue.steal())
  ///   object->invokeObjectMemberFunction();
  ///   
  /// // If using the object for a call:
  /// if (auto object = queue.steal())
  ///   // Passes object by reference:
  ///   functionUsingObject(*object);
  /// ~~~
  /// 
  /// Returns a reference to the top element in the queue.
  OptionalType steal() {
    auto top = Top.load(std::memory_order_relaxed);

    // The top variable must be loaded __before__ bottom, incase pop()'s from
    // the queue's thread have modified the queue.
    auto bottom = Bottom.load(std::memory_order_acquire);
    if (top >= bottom)
      return OptionalType{};

    // Here the object at top is fetched, and and update to Top is atempted,
    // since the top element is being stolen. __If__ the exchange succeeds,
    // then this method won a race with another thread to steal the element,
    // or if there is only a single element left, then it won a race between
    // other threads and the pop() method (potentially), or there was no race.
    // In summary, if the exchange succeeds, the object can be returned,
    // otherwise it can't.
    auto object    = make_optional(Objects[wrappedIndex(top)]);
    bool exchanged = Top.compare_exchange_strong(top                      ,
                                                 top + 1                  ,
                                                 std::memory_order_release);
    return exchanged ? object : OptionalType{};
  }

  /// Returns the number of elements in the queue. This __does not__ always
  /// return the actualy size, but an approximation of the size since both
  /// Top can be modified by another thread which steals.
  SizeType size() const noexcept { 
    return Bottom - Top;
  }

 private:
  /// This struct is used to dispatch based on whether the max number of
  /// objects in the queue is a power of 2 size.
  /// \tparam IsPowerOfTwo If the queue's max size is a power of 2.
  template <bool IsPowerOfTwo> struct SizeSelector {};

  //==--- Constants --------------------------------------------------------==//
  
  /// Defines if the maximum number of objects for the queue is a power of 2.
  static constexpr bool powerOf2Size = Math::isPowerOfTwo(MaxObjects);
  /// Defines a mask to used when the queue size is a power of 2, and is
  /// unsused when the size is not a power of 2.
  static constexpr SizeType wrapMask = MaxObjects - 1;

  /// Defines an alias for a fast size selector (max size is a power of 2).
  using FastSelector = SizeSelector<true>;
  /// Defines an alias for a slow size selector (max size is not a power of 2).
  using SlowSelector = SizeSelector<false>;

  //==--- Members ----------------------------------------------------------==//
  
  Container  Objects;       //!< Container of tasks.
  AtomicType Top      = 0;  //!< The index of the top element of the deque.
  Atomicype  Bottom   = 0;  //!< The index of the bottom element of the queue.
                     
  //==--- Methods ----------------------------------------------------------==//
  
  /// Gets the wrapped index for either the top or bottom of the queue.
  SizeType wrappedIndex(SizeType index) const noexcept {
    return wrappedIndexImpl(index, SizeSelector<powerOf2Size>{});
  }
  
  /// Overload of index update function for a power of 2 size queue.
  SizeType
  wrappedIndexImpl(SizeType index, FastSelector selector) const noexcept {
    return index & wrapMask;
  }

  /// Overload of index wrapping when the queue size is not a power of 2.
  SizeType
  wrappedIndexImpl(SizeType index, SlowSelector selector) const noexcept {
    return index % MaxObjects;
  }
};

}} // namespace Voxx::Conky