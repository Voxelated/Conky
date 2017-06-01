//==--- Conky/Concurrent/StaticStealableQueue.hpp ---------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  StaticStealableQueue.hpp
/// \brief This file provides the definition of a fixed size, lock-free, steable
///        queue.
//
//==------------------------------------------------------------------------==//

#pragma once

#include <Voxel/Math/Math.hpp>
#include <Voxel/Thread/Thread.hpp>
#include <array>
#include <atomic>
#include <experimental/optional>

namespace Voxx  {
namespace Conky {

/// The StaticStealableQueue is a fixed-size, concurrent, lock-free stealable
/// queue implementation. It is designed for a single thread to push and pop
/// onto and from the bottom of the queue, and any number of threads to steal
/// from the top of the queue.
/// 
/// The API provides by tryPush, push, pop, steal, and size. 
/// 
/// The push function will push an object onto the queue regardless of whether
/// the queue is full or not, and since the queue is circular, will therefore
/// overwrite the oldest object which was pushed onto the queue. It is faster
/// than tryPush, but must only be used when it's known that the queue is not
/// full.
/// 
/// The tryPush function calls push if the queue is not full.
/// 
/// Pop and steal will return a nullopt if the queue is empty, or if another
/// thread stole the last element before they got to it. Otherwise they will
/// return a pointer to the object.
/// 
/// To provide an API that does not easily introduce bugs, all returned objects
/// are __copies__, thus the queue is not suitable for large objects. This is
/// because providing a reference or pointer to an element introduces a
/// potential race between the use of that element and a push to the queue.
/// 
/// Consider if the queue is full. Calling ``tryPush`` returns false, as one
/// would expect, and calling ``steal`` returns the oldest element (say at
/// index 0), and increments top (to 1). Another call to ``tryPush`` then
/// succeeds because the queue is not at capacity, but it creates the new
/// element in the same memory location as the element just stolen. Therefore,
/// if a pointer is returned, there is a race between the stealing thread to use
/// the element pointed to, and the pushing thread overwriting the memory. Such
/// a scenario is too tricky to use correctly to include in the API. Therefore,
/// benchmark the queue for the required data type (generic benchmarking code
/// is provided in the test suite).
/// 
/// Also note that due to the multi-threaded nature of the queue, a call to
/// size only returns the size of the queue at that point in time, and it's
/// possible that other threads may have stolen from the queue between the time
/// at which size was called and then next operation.
/// 
/// This imlementation is based on the original paper by Chase and Lev:
/// 
///   [Dynamic circular work-stealing deque](
///     http://dl.acm.org/citation.cfm?id=1073974)
///     
/// without the dynamic resizing.
/// 
/// __Note:__ When using this class for per thread queues, ensure that each of
///           the per thread queues are aligned to avoid false sharing, i,e
///           
/// ~~~cpp
/// struct alignas(Voxx::Thread::nonDestructiveSize()) SomeClass {
///   StaticStealableQueue<SomeType, 1024> queue;
/// };
/// ~~~
/// 
/// \tparam Object      The type of the data in the queue.
/// \tparam MaxObjects  The maximum number of objects for the queue.
template <typename Object, uint32_t MaxObjects>
class StaticStealableQueue {
 public:
  ///==--- Aliases ---------------------------------------------------------==//
  
  /// Defines the type used for the queue top and bottom indices.
  using SizeType      = uint32_t;
  /// Defines the atomic type used for the index pointers.
  using AtomicType    = std::atomic<SizeType>;
  /// Defines the type of the objects in the queue.
  using ObjectType    = Object;
  /// Defines an optional typeto use when returning value types.
  using OptionalType  = std::experimental::optional<ObjectType>;
  /// Defines the type of container used to store the queue's objects.
  using ContainerType = std::array<ObjectType, MaxObjects>;

  //==--- Methods ----------------------------------------------------------==//
  
  /// Pushes an object onto the front of the queue, when the object is an
  /// rvalue reference type. This does not check if the queue is full, and hence
  /// it __will overwrite__ the least recently added element if the queue is
  /// full. 
  /// \param[in] args         The arguments used to construct the object.
  /// \tparam    ObjectArgs   The type of the arguments for object construction.
  template <typename... ObjectArgs>
  void push(ObjectArgs&&... args) {
    const auto bottom = Bottom.load(std::memory_order_relaxed);
    const auto index  = wrappedIndex(bottom);
    new (&Objects[index]) Object(std::forward<ObjectArgs>(args)...);

    // Ensure that the compiler does not reorder this instruction and the
    // setting of the object above, otherwise the object seems added (since the
    // bottom index has moved) but isn't (since it would not have been added).
    Bottom.store(bottom + 1, std::memory_order_release);
  }

  /// Tries to push an object onto the front of the queue.
  /// \param[in] objectArgs   The arguments used to construct the object.
  /// \tparam    ObjectArgs   The type of the arguments for object construction.
  /// Returns true if a new object was pushed onto the queue.
  template <typename... ObjectArgs>
  bool tryPush(ObjectArgs&&... args) {
    if (size() >= MaxObjects)
      return false;

    push(std::forward<ObjectArgs>(args)...);
    return true;
  }
  
  /// Pops an object from the front of the queue. This returns an optional
  /// type which holds either the object at the bottom of the queue (the most
  /// recently added object) or an invalid optional.
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
    using namespace std::experimental;
    auto bottom = Bottom.load(std::memory_order_relaxed) - 1;

    // Sequentially consistant memory ordering is used here to ensure that the
    // load to top always happens after the load to bottom above, and that the
    // compiler emits an __mfence__ instruction. Unfortunately, benchmarking has
    // shown that the mfence does degrade performance, but the code obviously
    // has to be correct.
    Bottom.store(bottom, std::memory_order_seq_cst);

    auto top = Top.load(std::memory_order_relaxed);
    if (top > bottom) {
      Bottom.store(top, std::memory_order_relaxed);
      return nullopt;
    }

    auto object = make_optional(Objects[wrappedIndex(bottom)]);
    if (top != bottom)
      return object;

    // If we are here there is only one element left in the queue, and there may
    // be a race between this method and steal() to get it. If this exchange is
    // true then this method won the race (or was not in one) and then the
    // object can be returned, otherwise the queue has already been emptied.
    bool exchanged = Top.compare_exchange_strong(top                      ,
                                                 top + 1                  ,
                                                 std::memory_order_release);
      
    // This is also a little tricky: If we lost the race, top will be changed to
    // the new value set by the stealing thread (i.e it's already incremented).
    // If it's incremented again then Bottom > Top when the last item was
    // actually just cleared. This is also the unlikely case -- since this path
    // is only executed when there is contention on the last element -- so the
    // const of the branch is acceptable.
    Bottom.store(top + (exchanged ? 1 : 0), std::memory_order_relaxed);

    return exchanged ? object : nullopt;
  }

  /// Steals an object from the top of the queue. This returns an optional type
  /// which is the object if the steal was successful, or a default constructed
  /// optional if not. Since this copies an object from the queue into the
  /// optional, it is less performant that getting a pointer, however, there is
  /// no API which can provide a reference an quarentee that the returned object
  /// is valid (if the underlying storage is a circular queue), so therefore no
  /// reference API is provided.
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
  /// Returns a pointer to the top (oldest) element in the queue, or nullptr.
  OptionalType steal() {
    using namespace std::experimental;
    // Top must be loaded before bottom, so we use acquire ordering since it
    // ensures that everyting below it stays below it.
    auto top = Top.load(std::memory_order_relaxed);

    // Top must always be set before bottom, so that bottom - top represents an
    // accurate enough (to prevent error) view of the queue size. Loads to
    // different address aren't reordered (i.e load load barrier)
    Thread::memoryBarrier();
    auto bottom = Bottom.load(std::memory_order_relaxed);

    if (top >= bottom) {
      return nullopt;
    }

    // Here the object at top is fetched, and and update to Top is atempted,
    // since the top element is being stolen. __If__ the exchange succeeds,
    // then this method won a race with another thread to steal the element,
    // or if there is only a single element left, then it won a race between
    // other threads and the pop() method (potentially), or there was no race.
    // In summary, if the exchange succeeds, the object can be returned,
    // otherwise it can't.
    // 
    // Also note that the object __must__ be created before the exchange to top,
    // otherwise there will potentially be a race to construct the object and
    // between a thread pushing onto the queue.
    // 
    // This introduces overhead when there is contention to steal, and the steal
    // is unsuccessful, but in the succesful case there is no overhead.
    auto object    = make_optional(Objects[wrappedIndex(top)]);
    bool exchanged = Top.compare_exchange_strong(top                      ,
                                                 top + 1                  ,
                                                 std::memory_order_release);

    return exchanged ? object : nullopt;
  }

  /// Returns the number of elements in the queue. This __does not__ always
  /// return the actualy size, but an approximation of the size since both
  /// Top can be modified by another thread which steals.
  SizeType size() const noexcept {
    return Bottom.load(std::memory_order_relaxed) -
           Top.load(std::memory_order_relaxed);
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
 
  // Note: The starting values are 1 since the element popping pops (bottom - 1)
  //       so if bottom = 0 to start, then (0 - 1) = size_t_max, and the pop
  //       function tries to access an out of range element.
  
  ContainerType Objects;       //!< Container of tasks.
  AtomicType    Top      = 1;  //!< The index of the top element.
  AtomicType    Bottom   = 1;  //!< The index of the bottom element. 
                     
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