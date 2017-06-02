//==--- Conky/Container/ThreadPool.hpp --------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  ThreadPool.hpp
/// \brief This file provides the definition of a thread pool which attempts to
///        balance the work load, and which should eventually support
///        overriding the balancing for high priority tasks.
//
//==------------------------------------------------------------------------==//

#pragma once

#include "Task.hpp"
#include "ThreadPoolException.hpp"
#include <Voxel/Conky/Concurrent/StaticStealableQueue.hpp>
#include <Voxel/Conky/Thread/Thread.hpp>
#include <Voxel/Thread/Thread.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <thread>
#include <vector>

namespace Voxx  {
namespace Conky {

/// The StealPolicy enum defines the stealing policy used by the thread pool.
/// See the benchmarks for the difference in performance between the policies.
enum class StealPolicy : uint8_t {
  /// Randomly select the queue from which to steal from. The random generator
  /// is fast (~2ns for a random number), but may result in collisions between
  /// threads (and hence contention on the element to steal), and may also
  /// generate the id of the worker (from which it just tried to pop but failed
  /// because the queue was empty).
  Random           = 0,
  /// Tries to steal from the nearest neighbor, increasing the neighbor distance
  /// until all neighbors are exhausted. For example, if the pool contains 4
  /// work queues, and 3 workers, and all threads fail to pop simultaneously 
  /// from their own queues at, the following steal sequence will occur:
  /// 
  /// | Thread         | Attempt 1 Q | Attempt 2 Q | Attempt 3 Q | 
  /// |:--------------:|:-----------:|:-----------:|:-----------:|
  /// | 1              | 2           | 3           | 0           |
  /// | 2              | 3           | 0           | 1           |
  /// | 3              | 0           | 1           | 2           |
  /// 
  /// This generally reduces contention when stealing, but not always as thread
  /// 1 may be on it's second steal attempt when thread 2 starts trying to steal
  /// then both will try and steal from 3.
  NearestNeighbour = 1,
  /// This uses the system topolgy information to attempt to steal based on the
  /// NUMA and cache properties of the system (i.e it walks up the cache sharing
  /// heirarchy trying to steal from the closest shared cache). This should give
  /// the best performance, especially for large systems, but is not yet
  /// implemented.
  Topological      = 2
};

/// The ThreadPool class creates a thread pool with one controlling thread
/// (which may be extended to be an interruptible worker thread) and (N - 1)
/// worker threads, wher N is the total number of threads in the pool. There is
/// a pool of tasks for each of the threads, including the controller thread.
/// If a thread's task queue is empty, it will try and steal from another
/// thread's pool of tasks.
/// 
/// The following is a list of features to be added:
/// 
/// 1.  Support for yielding and interrupting a task based on priority, without
///     incurring a context switch.
/// 
/// \tparam TasksPerQueue The maximum number of tasks which can be stored in
///                       each of the queues owned by a thread.
/// \tparam TaskAlignemnt The number of bytes to align each task to. This should
///                       be a multiple of the cache line size. The default is
///                       two cache lines as this is usually enough space for
///                       task arguments.
template <std::size_t TasksPerQueue                                         ,
          StealPolicy Policy        = StealPolicy::NearestNeighbour         ,
          std::size_t TaskAlignment = System::destructiveInterfaceSize() * 2>
class ThreadPool {
 public:
  //==--- Aliases ----------------------------------------------------------==//
  
  /// Defines the type of the tasks.
  using TaskType = Task<TaskAlignment>;

 private:
  //==--- Structs ----------------------------------------------------------==//
  
  /// The StealOverloader struct is used to overload the stealing implementation
  /// based on the provided policy.
  /// \tparam PolicyKind  The kind of the stealing policy.
  template <StealPolicy PolicyKind>
  struct StealOverloader {};

  //==--- Aliases ----------------------------------------------------------==//
  
  /// Defines the type of the stealing method selector.
  using StealSelector    = StealOverloader<Policy>;
  /// Defines the type of overloader for random stealing.
  using RandomSteal      = StealOverloader<StealPolicy::Random>;
  /// Defines the type of overloader for nearest neighbour stealing.
  using NNeighbourSteal  = StealOverloader<StealPolicy::NearestNeighbour>;
  /// Defines the type of overloader for topological stealing. 
  using TopologicalSteal = StealOverloader<StealPolicy::Topological>;

  //==--- Constants --------------------------------------------------------==//

  /// Defines the size of the alignment for the tasks queues. The alignment
  /// must be at least that of the tasks in the task queue, and large enough
  /// to avoid false sharing. 
  static constexpr std::size_t queueAlignment = 
    std::max(TaskAlignment, System::destructiveInterfaceSize());

  /// The TaskQueue defines a correctly aligned queue of tasks for a thread.
  struct alignas(queueAlignment) TaskQueue {
    //==--- Aliases --------------------------------------------------------==//
  
    /// Alias for the type of the queue.
    using QueueType = StaticStealableQueue<TaskType, TasksPerQueue>; 

    //==--- Members --------------------------------------------------------==//
    
    QueueType tasks; //!< The queue of tasks to be executed by workers.

    //==--- Con/destructors ------------------------------------------------==//
    
    /// Constructor -- uses the default.
    TaskQueue() noexcept = default;

    /// Move constructor -- deleted.
    TaskQueue(TaskQueue&&)      = delete;
    /// Copy constructor -- deleted.
    TaskQueue(const TaskQueue&) = delete;

    //== Operator overloads ------------------------------------------------==//
    
    /// Move assignment overload -- deleted.
    TaskQueue& operator=(TaskQueue&&)      = delete;
    /// Copy assignment operator -- deleted.
    TaskQueue& operator=(const TaskQueue&) = delete;
  };

 public:
  //==--- Aliases ----------------------------------------------------------==//

  /// Defines the type of container for the flags to control the worker threads.
  using FlagContainer   = std::vector<uint8_t>;
  /// Defines the type of container for the queues.
  using QueueContainer  = std::vector<TaskQueue>;
  /// Defines the type of thread container.
  using ThreadContainer = std::vector<std::thread>;

  //==--- Con-destruction --------------------------------------------------==//
  
  /// Constructor -- creates \p numThreads - 1 threads and starts processing
  /// tasks on them.
  /// \param[in] numThreads  The number of threads in the pool, including the
  ///                        manager thread.
  ThreadPool(std::size_t numThreads)
  :   Flags(numThreads), Threads(0), TaskQueues(numThreads) {
    Debug::catcher([numThreads] () {
      if (numThreads > System::CpuInfo::cores())
        throw ThreadPoolException(
          ThreadPoolException::Type::Oversubscription);
    });

    // The first thread is always the controlling thread.
    Voxx::Thread::setAffinity(0);
    makeWorkersRunnable();
    createWorkers(numThreads - 1);
  }

  /// Destructor -- joins the threads.
  ~ThreadPool() noexcept {
    stopThreads();
    join();
  }

  //==--- Methods ----------------------------------------------------------==//
  
  /// Tries to push an object onto the worker queue for the current thread. If
  /// the queue is full, then this will return false.
  /// \param[in]  callable      The callable object to invoke for the task.
  /// \param[in]  args          The arguments for the callable.
  /// \tparam     TaskCallable  The type of the callable task object.
  /// \tparam     TaskArgs      The types of the rguments for the task.
  template <typename TaskCallable, typename... TaskArgs>
  bool tryPush(TaskCallable&& callable, TaskArgs&&... args) {
    auto& tasks = TaskQueues[Thread::threadId].tasks;
    if (tasks.size() >= TasksPerQueue) {
      // co_await pop();
      return false;
    }

    tasks.push(std::forward<TaskCallable>(callable) ,
               std::forward<TaskArgs>(args)...      );
    return true;
  }
 
  /// Stops the threads from running.
  void stopThreads() noexcept {
    for (auto& flag : Flags) {
      flag = false;   
    }
  }

  /// Starts the threads running. This ensures that a worker thread is not
  /// running before creating another thread in its place. If the thread is
  /// running and has not been set to stop running, it's left to run. If it's
  /// been set to stop running and is still running, then it's set to stop, and
  /// waited on until it finishes, and a new thread is created in its place.
  void startThreads() noexcept {
    if (Thread::threadId == 0) {
      restartThreads();
    } else {
      auto restarter = std::thread([this] {
        ::Voxx::Thread::setAffinity(0);
        this->restartThreads();
      });
      restarter.join();
    }
  }

  /// Returns the number of threads which are running. This will always return a
  /// value of 1 or greater since the main/manager thread is alwasy running.
  std::size_t runningThreads() const noexcept {
    std::size_t count = 0;
    for (const auto& flag : Flags)
      count += flag;
    return count;
  }

  /// Waits until the thread pool has no work.
  void waitTillIdle() const noexcept {
    while (!isEmpty()) { /* Spin ... */ }
  }

  /// Returns true if the thread pool has no more work, otherwise it returns
  /// false.
  bool isEmpty() const noexcept {
    for (const auto& queue : TaskQueues)
      if (queue.tasks.size())
        return false;
    return true;
  }

 private:
  //==--- Members ----------------------------------------------------------==//
  
  FlagContainer   Flags;      //!< Flags to control the workers.
  ThreadContainer Threads;    //!< The threads for the pool.
  QueueContainer  TaskQueues; //!< The queues of tasks for each thread.
  
  //==--- Methods ----------------------------------------------------------==//

  /// Allows a worker thread with \p workerId to check if it must still run.
  /// \note Worker threads can only read their state, so the state controllers
  ///       are not atomic.
  bool runnable(std::size_t workerId) const noexcept {
    return Flags[workerId];
  }                                   

  /// Creates the worker threads.
  void createWorkers(std::size_t numThreads) {
    // TODO: Change this to get a proper thread index.
    for (std::size_t i = 0; i < numThreads; ++i)
      Threads.push_back(std::thread(&ThreadPool::process, this, i + 1));
  }

  /// Sets the worker flags to enable them to start running.
  void makeWorkersRunnable() noexcept {
    for (auto& flag : Flags) {
      flag = true;
    }
  }

  /// Joins the threads.
  void join() noexcept {
    if (Thread::threadId == 0) {
      for (auto& thread : Threads) {
        if (thread.joinable()) {
          thread.join();
        }
      }
    }
  }

  /// Restarts the threads. This shutsdown the thread, and then joins it before
  /// createing a new thread in the place of the old thread. It's designed to
  /// be safe, and is not expected that the pool's threads should be started and
  /// stopped often.
  void restartThreads() {
    if (Thread::threadId == 0) {
      std::size_t i = 0;
      Flags[0]      = true;
      for (auto& thread : Threads) {
        auto& runnableFlag = Flags[++i];

        // If the thread has not been set to stop, there is no point in creating
        // a new one, rather just leave the old one runnig.
        if (runnableFlag) {
          continue;
        }

        runnableFlag = false;
        if (thread.joinable()) {
          thread.join();
        }

        runnableFlag = true;
        thread = std::move(std::thread(&ThreadPool::process, this, i));
      }
    }
  }

  /// Processes tasks using the \p workerId thread and the \p workerId worker.
  /// \param[in] id The index to assign to the thread, and which identifies
  ///               the worker.
  void process(std::size_t workerId) {
    using namespace std::experimental;
    Voxx::Thread::setAffinity(workerId);

    // The global threadId, which can be used in client code to identify the
    // thread, is set to __id__ value so that when pushing to the thread pool
    // the correct worker can be accessed extremely quickly.
    if (Thread::threadId != workerId) {
      Thread::threadId = workerId;
    }

    while (runnable(workerId)) {
      processImpl(workerId);
    }
  }

  /// Process function for the controller thread.
  void processImpl(std::size_t workerId) {
    if (auto task = TaskQueues[workerId].tasks.pop()) {
      task->executor->execute();
      return;
    }
    steal(workerId, StealSelector());
  }

  /// Defines the implemenation of the stealing method when the stealing
  /// policy is to steal randomly from a task queue from anoter thread.
  /// \param[in] workerId   The id of the worker which is stealing.
  /// \param[in] tag        The tag used to select this overload.
  void steal(std::size_t workerId, RandomSteal tag) {
    auto& tasks = TaskQueues[Math::randint(0, TaskQueues.size())].tasks;
    if (auto task = tasks.steal()) {
      task->executor->execute();
    }
  }

  /// Defines the implemenation of the stealing method when the stealing
  /// policy is to steal from the nearest neighbour.
  /// \param[in] workerId   The id of the worker which is stealing.
  /// \param[in] tag        The tag used to select this overload.
  void steal(std::size_t workerId, NNeighbourSteal tag) {
    for (std::size_t offset = 1; offset < TaskQueues.size(); ++offset) {
      auto& tasks = TaskQueues[(workerId + offset) % TaskQueues.size()].tasks;
      if (auto task = tasks.steal()) {
        task->executor->execute();
        return;
      }
    }
  }
};

}} // namespace Voxx::Conky