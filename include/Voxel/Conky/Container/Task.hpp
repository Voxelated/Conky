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
#include <Voxel/Utility/Debug.hpp>
#include <atomic>
#include <type_traits>

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

  /// The clone method enables copying/moving the executor.
  /// \param[in]  storage   A pointer to the storage to clone the executor into.
  virtual TaskExecutor* clone(void* storage) noexcept = 0;

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
  //==--- Aliases ----------------------------------------------------------==//
  
  /// Defines an alias for the type of the executor.
  using Invokable = Function::Callable<CallableType, Args...>;
  /// Defines the type of this struct.
  using SelfType  = Executor<CallableType, Args...>;

  /// Returns true if \t is not a task.
  /// \tparam T The type to test if not a task.
  template <typename T> 
  static constexpr bool isNotTask =
    !std::is_same<std::decay_t<T>, SelfType>::value;

  /// Constructor -- forwards the callable and the arguments to the invoker.
  /// \param[in]  callable  The callable object to store.
  /// \param[in]  args      The arguments for the callable to store.
  /// \tparam     CType     The type of the callable.
  /// \tparam     ATypes    The types of the args.
  /// \tparam     Enabled   Used to enable this constructor overload.
  template <typename CType      ,
            typename... AType   ,
            typename    Enabled = std::enable_if_t<isNotTask<CType>>>
  Executor(CType&& callable, AType&&... args) noexcept
  : Invoker{std::forward<CallableType>(callable), std::forward<Args>(args)...}
  {}

  /// Copy constructor -- copies \p other into this.
  /// \param[in]  other   The other executor to copy.
  Executor(const SelfType& other) noexcept : Invoker(other.Invoker) {}

  /// Move constructor -- moves \p other into this.
  /// \param[in]  other   The other executor to copy.
  Executor(SelfType&& other) noexcept : Invoker(std::move(other.Invoker)) {} 

  //==--- Overrides --------------------------------------------------------==//
  
  /// Override of the execute method to run the executor.
  virtual void execute() final override {
    Invoker();
  }

  /// Override of the clone method to copy this class into the provided 
  /// \p storage.
  /// \param[in]  storage   The storage to clone into.
  virtual SelfType* clone(void* storage) noexcept final override {
    new (storage) SelfType(*this);
    return reinterpret_cast<SelfType*>(storage);
  }

 private:
  Invokable Invoker;  //!< The object which envokes the executor.
};

/// A task is essentially some body of work which needs to be done. The task
/// should be given an alignment which is a multiple of the cache line size so
/// that there is no false sharing of tasks when they are used across threads.
/// 
/// There is some overhead for the task, which seems unavoidable. In order to
/// enable __any__ callable object to be used as a task, the abstract base
/// class TaskExecutor is required, and therefore the task must store a pointer.
/// (This is not really a problem in terms of storage overhead since tasks
/// should be cache line aligned, and the 8 bytes are usually only a fraction
/// of the cache line size), but it does reduce the available storage for
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
/// the the cost is approximately 1.1ns any body of work executed through a
/// task vs 0.25 if inlined, which is an acceptable cost for the felxibility of
/// the genericy of the tasks.
/// 
/// The above limitations are not significant, since the __correct__ use of
/// tasks is __not__ to have trivial workloads. Even a task with a ~30ns
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

  /// Returns true if \t is not a task.
  template <typename T>
  static constexpr bool isNotTask = 
    !std::is_same<std::decay_t<T>, Task>::value;

  //==--- Members ----------------------------------------------------------==//
  
  TaskExecutor* executor   = nullptr; //!< The executor to run the task.
  uint32_t      parentId   = 0;       //!< Id of the task's parent.
  CounterType   unfinished = 0;       //!< Counter for unfinished tasks.
  char          storage[size];        //!< Additional storage.

  //==--- Con/destruction --------------------------------------------------==//
 
  /// Default constructor -- uses the default values of the task. 
  Task() noexcept {}

  /// Copy constructor -- Clones the executor and copies the task state.
  /// This will check against the executor being a nullptr in debug mode.
  /// \param[in]  other   The other task to copy from.
  Task(const Task& other) noexcept {
    debugAssertValid(other);
    executor = other.executor->clone(&storage);
  }

  /// Move constructor -- Clones the executor and moves the task state.
  /// This will check against the executor being a nullptr in debug mode.
  /// \param[in]  other   The other task to move from.
  Task(Task&& other) noexcept {
    debugAssertValid(other);
    executor = other.executor->clone(&storage);   
  }

  /// Constructs the executor by storing the callable and the callable's
  /// arguments in the additional storage for the task.
  /// 
  /// The constructor is only enabled if CalllableType is not a Task.
  /// 
  /// \todo Add support for when the callable and it's arguments are too large
  ///       for the additional available storage.
  ///       
  /// \param[in]  callable      The callable object to store.
  /// \param[in]  args          The arguments to store.
  /// \tparam     CallableType  The type of the callable object.
  /// \tparam     Args          The type of the arguments for the callable.
  /// \tparam     Enabled       Used to enable this overload.
  template <typename    CallableType,
            typename... Args        ,
            typename    Enabled     = std::enable_if_t<isNotTask<CallableType>>>
  Task(CallableType&& callable, Args&&... args) noexcept {
    setExecutor(std::forward<CallableType>(callable),
                std::forward<Args>(args)...         );
  }

  //==--- Operator overloads -----------------------------------------------==//
  
  /// Copy assignment overload -- Clones the executor and copies the task state.
  /// This will check against the executor being a nullptr in debug mode.
  /// \param[in] other  The other task to copy.
  /// Returns a reference to the modified task.
  Task& operator=(const Task& other) noexcept {
    debugAssertValid(other);
    executor = other.executor->clone(&storage);
    return *this;
  }

  /// Move assignment overload -- Clones the executor and moves the task state.
  /// This will check against the executor being a nullptr in debug mode.
  /// \param[in] other  The other task to move from.
  /// Returns a reference to the modified task.
  Task& operator=(Task&& other) noexcept {
    debugAssertValid(other);
    executor = other.executor->clone(&storage);
    return *this;
  }
  
  //==--- Methods ----------------------------------------------------------==//

  /// Sets the executor of the task.
  /// \param[in]  callable      The callable object to store.
  /// \param[in]  args          The arguments to store.
  /// \tparam     CallableType  The type of the callable object.
  /// \tparam     Args          The type of the arguments for the callable.
  template <typename CallableType, typename... Args>
  void setExecutor(CallableType&& callable, Args&&... args) noexcept {
    using ExecutorType = Executor<CallableType, Args...>;
    new (&storage) ExecutorType(std::forward<CallableType>(callable),
                                std::forward<Args>(args)...         );
    executor = reinterpret_cast<ExecutorType*>(&storage);
  }

 private:
  /// Checks if the \p task is valid. In release mode this is disabled. In
  /// debug mode this will terminate if the executor in \p task is a nullptr.
  /// \param[in]  task  The task to check the validity of.
  void debugAssertValid(const Task& task) const noexcept {
    Debug::catcher([&task] {
      if (task.executor == nullptr) {
        VoxxAssert(false, "Task executor is a nullptr!");
      }
    });
  }
};

}} // namespace Voxx::Conky