//==--- Conky/Container/Task.hpp --------------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  Task.hpp
/// \brief This file provides the definition of a task class.
//
//==------------------------------------------------------------------------==//

#pragma once

#include <Voxel/Function/Callable.hpp>
#include <atomic>

namespace Voxx  {
namespace Conky {

/// The TaskExecutor struct is a base class which enable a thread pool to be
/// able to __store__ and execute a callable object with __any__ signature,
/// without intruding on client code __at all__, i.e the client does not need
/// to consider the tasking framework when creating a class that will become a
/// task.
struct TaskExecutor {
  //==--- Con/destructors --------------------------------------------------==//
  
  /// Virtual destructor to avoid incorrect deletion of base class.
  virtual ~TaskExecutor() {}

  //==--- Methods ----------------------------------------------------------==//

  /// The execute method defines the interface for task execution.
  virtual void execute() = 0;
};

/// The Executor class is used to store a generic executor, and is simply a
/// wrapper around a Callable which derives from TaskExecutor which allows
/// it to be stored in and run by a Task.
/// \tparam   CallableType  The type of the callable to store.
/// \tparam   Args          The type of the callable's arguments to store.
template <typename CallableType, typename... Args>
struct Executor final : public TaskExecutor {
  /// Defines an alias for the type of the executor.
  using Invokable = Function::Callable<CallableType, Args...>;

  /// Constructor -- forwards the callable and the arguments to the invoker.
  /// \param[in]  callable  The callable object to store.
  /// \param[in]  args      The arguments for the callable to store.
  template <typename CType, typename... AType>
  Executor(CType&& callable, AType&&... args)
  : Invoker{std::forward<CallableType>(callable), std::forward<Args>(args)...}
  {}

  //==--- Overrides --------------------------------------------------------==//
  
  /// Override of the execute method to run the executor.
  virtual void execute() final override {
    Invoker();
  }

 private:
  Invokable Invoker;  //!< The object which envokes the executor.
};

/// A task is essentially a callable object -- it represents some body of work
/// which needs to be done. The task should be given an alignment which is a
/// multiple of the cache line size so that there is no false sharing of tasks.
/// 
/// There is some overhead for the task, which seems unavoidable. In order to
/// enable __any__ callable object to be used as a task, the abstract base
/// class TaskExecutor is required, and therefore the task must store a pointer.
/// (This is not really a problem in terms of storage overhead since tasks
/// should be cache line aligned), but it does reduce the available storage for
/// the callable and its arguments (therefore only a problem if the callable has
/// many arguments).
/// 
/// The callable and its arguments are stored in the extra space remaining on
/// the cache line(s), so that tasks can be moved around.
/// 
/// Executing the task then requires invoking the stored callable through the
/// base TaskExecutor class. Tests have shown that the compiler is usually able
/// to remove the virtual function indirection, and the performance is usually
/// the same as a __non inlined__ function call. The major drawback therefore,
/// is the loss of the ability for the compiler to inline. Benchmarks show that
/// the the cost is approximately 1.1ns vs 0.25 for an inlined function call,
/// which is certainly an acceptable cost for the felxibility of the genericy of
/// the tasks.
/// 
/// The above limitations are not significant, since the __correct__ use of
/// tasks is __not__ to have non-trivial workloads. Even a task with a ~30ns
/// workload only incurrs a ~2.5% overhead compared to if the task body was
/// executed inline.
/// 
/// \tparam Alignment   The aligment for the task.
template <std::size_t Alignment>
struct alignas(Alignment) Task {
  //==--- Aliases ----------------------------------------------------------==//
  
  /// Defines the type of the unfinished counter.
  using CounterType = std::atomic<uint32_t>;

  //==--- Constants --------------------------------------------------------==//
  
  /// Defines the size of the buffer for the storage.
  static constexpr std::size_t size =
    Alignment - sizeof(TaskExecutor*) - sizeof(uint32_t) - sizeof(CounterType);

  //==--- Con/destruction --------------------------------------------------==//
  
  Task() {}
  
  /// Constructs the executor by storing the callable and the callable's
  /// arguments in the additional storage for the task.
  /// \todo Add support for when the callable and it's arguments are too large
  ///       for the additional available storage.
  /// \param[in]  callable      The callable object to store.
  /// \param[in]  args          The arguments to store.
  /// \tparam     CallableType  The type of the callable object.
  /// \tparam     Args          The type of the arguments for the callable.
  template <typename CallableType, typename... Args>
  Task(CallableType&& callable, Args&&... args) {
    setExecutor(std::forward<CallableType>(callable),
                std::forward<Args>(args)...         );
  }

  /// Sets the executor of the task.
  /// \param[in]  callable      The callable object to store.
  /// \param[in]  args          The arguments to store.
  /// \tparam     CallableType  The type of the callable object.
  /// \tparam     Args          The type of the arguments for the callable.
  template <typename CallableType, typename... Args>
  void setExecutor(CallableType&& callable, Args&&... args) {
    using ExecutorType = Executor<CallableType, Args...>;
    new (&storage) ExecutorType(std::forward<CallableType>(callable),
                                std::forward<Args>(args)...         );
    executor = reinterpret_cast<ExecutorType*>(&storage);
  }

  //==--- Members ----------------------------------------------------------==//
  
  TaskExecutor* executor   = nullptr; //!< The executor to run the task.
  uint32_t      parentId   = 0;       //!< Id of the task's parent.
  CounterType   unfinished = 0;       //!< Counter for unfinished tasks.
  char          storage[size];        //!< Additional storage.
};

}} // namespace Voxx::Conky