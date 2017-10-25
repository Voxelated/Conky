//==--- Conky/Task/Task.hpp -------------------------------- -*- C++ -*- ---==//
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

#ifndef VOXX_CONKY_TASK_TASK_HPP
#define VOXX_CONKY_TASK_TASK_HPP

#include <Voxel/Function/Callable.hpp>
#include <Voxel/Meta/Meta.hpp>
#include <Voxel/Utility/Debug.hpp>
#include <atomic>
#include <type_traits>

namespace Voxx::Conky {

/// The Executor struct is a base class which enables a generic callable with
/// __any__ signature to be stored and then executed. This enables callabacks
/// and delegates to be non-intrusive in client code.
struct Executor {
  //==--- Con/destructors --------------------------------------------------==//
  
  /// Virtual destructor to avoid incorrect deletion of base class.
  virtual ~Executor() {}

  //==--- Methods ----------------------------------------------------------==//

  /// The clone method enables copying/moving the executor.
  /// \param[in]  storage   A pointer to the storage to clone the executor into.
  /// Returns a pointer to the cloned task executor which has been cloned into
  /// the provided \p storage.
  virtual Executor* clone(void* storage) noexcept = 0;

  /// The execute method defines the interface for task execution.
  virtual void execute() = 0;
};

/// The StorableExecutor implements the Executor interface and allows a callable
/// object with any signature to be stored using a pointer to the base Executor
/// interface. The type information is retained in the template parameters which
/// allow the callable to be executed through the base class pointer.
/// 
/// The intended usage is to store callables with different signatures in a
/// container. StorableExecutors should also be built using
/// the ``makeStorableExecutor()``, ``makeUniqueStorableExecutor()``,
/// and ``makeSharedStorableExecutor()`` functions.
/// 
/// Example usage:
/// 
/// ~~~cpp
/// using ExecutorContainer = std::vector<std::unique_ptr<Executor>>;
/// 
/// ExecutorContainer executors(0);
/// 
/// executors.emplace_back(std::move(
///   makeUniqueStorableExecutor([] (int a, int b) { return a + b; }));
/// executors.emplace_back(std::move(
///   makeUniqueStorableExecutor([] (auto&&... as) { someFunction(as...) }));
/// 
/// executors.front().execute();
/// executors.back().execute();
/// ~~~
/// 
/// This is less efficient as it wraps the Function::Callable class and is
/// invoked through a virtual function call. Tests have shows than the compiler
/// can devirtualize the execution pretty well, however, there is a loss of
/// performance due to the compiler not being able to inline the function call.
/// Tests have also shown that this is as fast as using std::function, so the
/// performance is likely good enough given the added advantage of being able
/// to store the executor though the base class.
/// 
/// The intended usage is anywhere that a container of generic callables is
/// required, i.e. for a thread pool which stores tasks to be executed.
/// 
/// \tparam   CallableType  The type of the callable to store.
/// \tparam   Args          The type of the callable's arguments to store.
template <typename CallableType, typename... Args>
struct StorableExecutor final : public Executor {
  //==--- Aliases ----------------------------------------------------------==//
  
  /// Defines an alias for the type of the executor.
  using Invokable = Function::Callable<CallableType, Args...>;
  /// Defines the type of this struct.
  using SelfType  = StorableExecutor<CallableType, Args...>;

 private:
  /// Returns true if \t is not an executor.
  /// \tparam T The type to test if not a task.
  template <typename T> 
  static constexpr bool isNotExecutor = 
    !std::is_same_v<std::decay_t<T>, SelfType>;

  /// Defines the type for enabling the constructor if the type is not an
  /// executor.
  /// \tparam   T   The type to determine the enabling from.
  template <typename T>
  using EnableType = std::enable_if_t<isNotExecutor<T>>;

 public:
  //==--- Con/destruction --------------------------------------------------==//

  /// Constructor -- forwards the callable and the arguments to be stored.
  /// \param[in]  callable  The callable object to store.
  /// \param[in]  args      The arguments for the callable to store.
  /// \tparam     CType     The type of the callable.
  /// \tparam     ATypes    The types of the args.
  /// \tparam     Enabled   Used to enable this constructor overload.
  template <typename CType      ,
            typename... AType   ,
            typename    Enabled = EnableType<CType>>
  StorableExecutor(CType&& callable, AType&&... args) noexcept
  : Invoker{std::forward<CallableType>(callable), std::forward<Args>(args)...}
  {}

  /// Copy constructor -- copies \p other into this.
  /// \param[in]  other   The other executor to copy.
  StorableExecutor(const SelfType& other) noexcept
  : Invoker(other.Invoker) {}

  /// Move constructor -- moves \p other into this.
  /// \param[in]  other   The other executor to copy.
  StorableExecutor(SelfType&& other) noexcept
  : Invoker(std::move(other.Invoker)) {} 

  //==--- Overrides --------------------------------------------------------==//
  
  /// Override of the execute method to run the executor.
  virtual void execute() final override {
    Invoker();
  }

  /// Override of the clone method to copy this class into the provided 
  /// \p storage using placement new, thus incurring no allocation overhead.
  /// \param[in]  storage   The storage to clone into.
  /// Returns a pointer to the executor which has been cloned into the \p
  /// storage provided.
  virtual SelfType* clone(void* storage) noexcept final override {
    new (storage) SelfType(*this);
    return reinterpret_cast<SelfType*>(storage);
  }

 private:
  Invokable Invoker;  //!< The object which envokes the executor.
};

/// Makes a storable executor.
/// \tparam   CallableType  The type of the callable to store.
/// \tparam   Args          The type of the callable's arguments to store.
template <typename CallableType, typename... Args>
auto makeStorableExecutor(CallableType&& callable, Args&&... args) {
  return StorableExecutor<CallableType, Args...>(
           std::forward<CallableType>(callable), std::forward<Args>(args)...);
}

/// Makes a std::unique_ptr to a storable executor.
/// \tparam   CallableType  The type of the callable to store.
/// \tparam   Args          The type of the callable's arguments to store.
template <typename CallableType, typename... Args>
auto makeUniqueStorableExecutor(CallableType&& callable, Args&&... args) {
  return std::make_unique<StorableExecutor<CallableType, Args...>>(
           std::forward<CallableType>(callable), std::forward<Args>(args)...);
}

/// Makes a std::shared_ptr to a storable executor.
/// \tparam   CallableType  The type of the callable to store.
/// \tparam   Args          The type of the callable's arguments to store.
template <typename CallableType, typename... Args>
auto makeSharedStorableExecutor(CallableType&& callable, Args&&... args) {
  return std::make_shared<StorableExecutor<CallableType, Args...>>(
           std::forward<CallableType>(callable), std::forward<Args>(args)...);
}

/// A task is essentially some body of work which needs to be done. The task
/// should be given an alignment which is a multiple of the cache line size so
/// that there is no false sharing of tasks when they are used across threads.
/// 
/// There is some overhead for the task, which seems unavoidable. In order to
/// enable __any__ callable object to be used as a task, the abstract base
/// class TaskExecutor is required, and therefore the task must store a pointer.
/// (This is not really a problem in terms of storage overhead since tasks
/// should be cacheline aligned, and the 8 bytes are usually only a fraction
/// of the cacheline size), but it does reduce the available storage for
/// the callable and its arguments (therefore only a problem if the callable has
/// many arguments).
/// 
/// The callable and its arguments are stored in the extra space remaining on
/// the cache line(s), so that tasks can be moved around.
/// 
/// Executing the task then requires invoking the stored callable through the
/// base TaskExecutor class. Tests have shown that the compiler is usually able
/// to remove the virtual function indirection (i.e devirtualize the execution),
/// and the performance is usually the same as a __non inlined__ function call.
/// The major drawback therefore, is the loss of the ability for the compiler to
/// inline the function call. Benchmarks show that the the cost is approximately
/// 1.1ns (4.0Ghz)for any body of work executed through a task vs 0.25ns if
/// inlined (this obviously depends on what is being executed), which is an
/// acceptable cost for the felxibility of the genericy of the tasks.
/// 
/// The above limitations are not significant, since the __correct__ use of
/// tasks is __not__ to have trivial workloads. Even a task with a ~30ns
/// workload only incurrs a ~2.5% overhead compared to if the task body was
/// executed inline.
/// 
/// \tparam Alignment   The aligment for the task.
/// 
/// \todo Add support for providing an allocator for task allocation if the 
///       executor or callable are too large to the buffer.
template <std::size_t Alignment>
struct alignas(Alignment) Task {
  /// The Id struct is used to store an identifier for a parent task as well as
  /// the current offset into the Task's data buffer.
  struct Id {
    //==--- Constants ------------------------------------------------------==//
    
    /// Defines the number of bits used to store the offset into the buffer.
    static constexpr std::size_t offsetBits = 9;
    /// Defines the number of bits used to store the value of the id.
    static constexpr std::size_t valueBits  = sizeof(uint32_t) * 8 - offsetBits;

    //==--- Members --------------------------------------------------------==//
    
    uint32_t offset : offsetBits; //!< The offset into the task storage.
    uint32_t value  : valueBits ; //!< The value of the id.
    
    //==--- Methods --------------------------------------------------------==//
    
    /// Operator to allow the Id type to be treated as a uint32_t, where the 
    /// value of the id is returned.
    operator uint32_t() const { return value; }
  };

  //==--- Aliases ----------------------------------------------------------==//
  
  /// Defines the type of the unfinished counter.
  using CounterType = std::atomic<uint32_t>;

  //==--- Constants --------------------------------------------------------==//
  
  /// Defines the size of the buffer for the storage.
  static constexpr std::size_t bufferSize =
    Alignment - sizeof(Executor*) - sizeof(uint32_t) - sizeof(CounterType);

 private:
  /// Returns true if T is not a task.
  template <typename T>
  static constexpr bool isNotTask = !std::is_same_v<std::decay_t<T>, Task>;

  /// Defines the type for enabling the non-task constructor.
  /// \tparam   T   The type to determine the enabling from.
  template <typename T>
  using EnableType = std::enable_if_t<isNotTask<T>>;

 public:
  //==--- Con/destruction --------------------------------------------------==//
 
  /// Default constructor -- uses the default values of the task. 
  Task() noexcept = default;

  /// Copy constructor -- Clones the executor and copies the task state.
  /// This will check against the executor being a nullptr in debug mode.
  /// \param[in]  other   The other task to copy from.
  Task(const Task& other) noexcept {
    debugAssertValid(other);
    Exe = other.Exe->clone(&Storage);
  }

  /// Move constructor -- Clones the executor and moves the task state.
  /// This will check against the executor being a nullptr in debug mode.
  /// \param[in]  other   The other task to move from.
  Task(Task&& other) noexcept {
    debugAssertValid(other);
    Exe       = other.Exe->clone(&Storage);
    other.Exe = nullptr;
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
            typename    Enabled     = EnableType<CallableType>>
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
    debugAssertValid(other.Exe);
    Exe = other.Exe->clone(&Storage);
    return *this;
  }

  /// Move assignment overload -- Clones the executor and moves the task state.
  /// This will check against the executor being a nullptr in debug mode.
  /// \param[in] other  The other task to move from.
  /// Returns a reference to the modified task.
  Task& operator=(Task&& other) noexcept {
    debugAssertValid(other.Exe);
    Exe       = other.Exe->clone(&Storage);
    other.Exe = nullptr;
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
    using ExecutorType = StorableExecutor<CallableType, Args...>;
    assertLargeEnoughBuffer<CallableType, Args...>();
    
    new (&Storage) ExecutorType(std::forward<CallableType>(callable),
                                std::forward<Args>(args)...         );
    Exe = reinterpret_cast<ExecutorType*>(&Storage);

    // Set the offset into the storage buffer:
    ParentId.value = sizeof(ExecutorType);
  }

  /// Executes the task.
  void execute() { 
    Exe->execute();
    if (Continuation != nullptr) {
      Continuation->execute();
    }
  }

  /// Adds a continuations to the task which will be run after the task has
  /// executed.
  template <typename ContCallable, typename... ContArgs>
  void then(ContCallable&& continuation, ContArgs&&... args) {
    using ContinuationType = StorableExecutor<ContCallable, ContArgs...>;

    constexpr auto continuationSize = sizeof(ContinuationType);
    const auto     remainingSpace   = bufferSize - ParentId.value;
    if (continuationSize > remainingSpace) {
      // TODO: Allocate memory for the continuation ...
    }

    void* storage = &Storage + ParentId.value;
    new (storage) ContinuationType(std::forward<ContCallable>(continuation),
                                   std::forward<ContArgs>(args)...         );
    Continuation    = reinterpret_cast<ContinuationType*>(storage);
    ParentId.value += continuationSize;
  }

 private:
  //==--- Members ----------------------------------------------------------==//
  
  Executor*    Exe             = nullptr; //!< The executor to run the task.
  Executor*    Continuation    = nullptr; //!< The continuation to run.
  CounterType  UnfinishedTasks = 0;       //!< Counter for unfinished tasks.
  Id           ParentId;                  //!< Id of the task's parent.
  char         Storage[bufferSize];       //!< Additional storage.
  
  //==--- Methods ----------------------------------------------------------==//
  
  /// Checks if the \p executor is valid. In release mode this is disabled. In
  /// debug mode this will terminate if the \p executor is a nullptr.
  /// \param[in]  executor  The executor to check the validity of.
  void debugAssertValid(const Executor* executor) const noexcept {
    Debug::catcher([&executor] {
      if (executor == nullptr) {
        VoxxAssert(false,
                   "Task being copied has an invalid (nullptr) executor!");
      }
    });
  }

  /// Asserts that the size of a parameter pack can fit into the storage buffer.
  /// \tparam Ts The types to check the validity of the size for.
  template <typename... Ts>
  void assertLargeEnoughBuffer() {
    static_assert(packByteSize<Ts...> <= bufferSize       ,
                  "Task callback requires too much memory");
  }
};

} // namespace Voxx::Conky

#endif // VOXX_CONKY_TASK_TASK_HPP