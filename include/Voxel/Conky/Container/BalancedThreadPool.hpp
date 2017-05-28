//==--- Conky/Container/BalancedThreadPool.hpp ------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  BalancedThreadPool.hpp
/// \brief This file provides the definition of a balanced thread pool -- one
///        that attempts to load balance the work across the assigned cores.
//
//==------------------------------------------------------------------------==//

#pragma once

#include "Task.hpp"
#include "ThreadPoolException.hpp"
#include <Voxel/Conky/Concurrent/StaticStealableQueue.hpp>
#include <Voxel/Conky/Thread/Thread.hpp>
#include <Voxel/Thread/Thread.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <Voxel/Utility/Debug.hpp>
#include <thread>
#include <vector>

#include <mutex>

namespace Voxx  {
namespace Conky {

/// The BalancedThreadPool creates worker threads, and each of the worker
/// threads has it's own pool of tasks. Tasks can be stolen from another thread
/// if the other thread has no more tasks on its queue.
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
template <std::size_t TasksPerQueue ,
          std::size_t TaskAlignment = System::destructiveInterfaceSize() * 2>
class BalancedThreadPool {
 public:
  //==--- Aliases ----------------------------------------------------------==//
  
  /// Defines the type of the tasks.
  using TaskType = Task<TaskAlignment>;

 private:
  //==--- Constants --------------------------------------------------------==//
 
  static constexpr std::size_t workerAlignment = 
    std::max(TaskAlignment, System::destructiveInterfaceSize());

  /// The Worker struct contains a queue of tasks for the worker to execute,
  /// and a flag which allows it to be interrupted/suspended, etc. The alignment
  /// of the struct ensures that there is no false sharing between workers.
  struct alignas(workerAlignment) Worker {
    //==--- Aliases --------------------------------------------------------==//
    
    Worker() {}
    
    /// Alias for the type of the object queue.
    using TaskQueue = StaticStealableQueue<TaskType, TasksPerQueue>; 
    /// Alias for the type of the flag used for controlling the worker.
    using FlagType  = std::atomic<bool>;

    //==--- Members --------------------------------------------------------==//
    
    TaskQueue tasks;            //!< The queue of tasks for the worker to run.
    FlagType  runnable = false; //!< A flag to control the worker.
  };

 public:
  //==--- Aliases ----------------------------------------------------------==//

  using WorkerPool      = std::vector<Worker>;
  /// Defines the type of thread container.
  using ThreadContainer = std::vector<std::thread>;

  //==--- Con-destruction --------------------------------------------------==//
  
  /// Constructor -- creates the threads, and starts them running.
  /// \param[in] numThreads  The number of threads to create.
  BalancedThreadPool(std::size_t numThreads)
  :   Threads(0), Workers(numThreads) {
    Debug::catcher([numThreads] () {
      if (numThreads > System::CpuInfo::cores())
        throw ThreadPoolException(
          ThreadPoolException::Type::Oversubscription);
    });

    // The first thread is always the controlling thread.
    ::Voxx::Thread::setAffinity(0);

    createWorkers(numThreads - 1);
    start();
  }

  /// Destructor -- joins the threads.
  ~BalancedThreadPool() noexcept {
    shutdown();
    join();
  }

  //==--- Methods ----------------------------------------------------------==//
  
  /// Pushes an object onto the worker queue for the current thread. This will
  /// contiue to try and push until the push is successful.
  /// \param[in]  callable      The callable object to invoke for the task.
  /// \param[in]  args          The arguments for the callable.
  /// \tparam     TaskCallable  The type of the callable task object.
  /// \tparam     TaskArgs      The types of the rguments for the task.
  template <typename TaskCallable, typename... TaskArgs>
  void push(TaskCallable&& callable, TaskArgs&&... args) {
    while (!tryPush(std::forward<TaskCallable>(callable),
                    std::forward<TaskArgs>(args)...     )) {
      // Spin ..
      // Maybe yield 
    }
  }

  /// Tries to push an object onto the worker queue for the current thread. If
  /// the queue is full, then this will return false.
  /// \param[in]  callable      The callable object to invoke for the task.
  /// \param[in]  args          The arguments for the callable.
  /// \tparam     TaskCallable  The type of the callable task object.
  /// \tparam     TaskArgs      The types of the rguments for the task.
  template <typename TaskCallable, typename... TaskArgs>
  bool tryPush(TaskCallable&& callable, TaskArgs&&... args) {
    auto& tasks = Workers[Thread::threadId].tasks;
    if (tasks.size() == TasksPerQueue)
      return false;

    tasks.push(std::forward<TaskCallable>(callable),
               std::forward<TaskArgs>(args)...     );
    return true;
  }
 
  /// Stops the threads. Currently this just joins them, but really this should
  /// interrupt the threads if they are running.
  void shutdown() noexcept {
    for (auto& worker : Workers)      
      worker.runnable.store(false, std::memory_order_relaxed);
  }

  /// Starts the threads running.
  void start() noexcept {
    for (auto& worker : Workers)
      worker.runnable.store(true, std::memory_order_relaxed);
  }

 private:
  ThreadContainer Threads;  //!< The threads for the pool.
  WorkerPool      Workers;  //!< Per thread pools of objects.
                                            
  /// Creates the worker threads.
  void createWorkers(std::size_t numThreads) {
    // TODO: Change this to get a proper thread index.
    for (std::size_t i = 0; i < numThreads; ++i)
      Threads.push_back(std::thread(&BalancedThreadPool::process, this, i + 1));
  }

  /// Joins the threads.
  void join() noexcept {
    if (Thread::threadId == 0)
      for (auto& thread : Threads)
        thread.joinable() ? thread.join() : thread.detach();
  }

  /// Processes tasks using the \p threadId thread and the \p threadId worker.
  /// \param[in] id The index to assign to the thread, and which identifies
  ///               the worker.
  void process(std::size_t id) {
    ::Voxx::Thread::setAffinity(id);

    // The global threadId, which can be used in client code to identify the
    // thread, is set to __id__ value so that when pushing to the thread pool
    // the correct worker can be accessed extremely quickly.
    Thread::threadId  = id;
    const auto maxId  = Workers.size();
    auto&      worker = Workers[id];
    TaskType*  task   = nullptr;

    while (worker.runnable.load(std::memory_order_relaxed)) {
      if ((task = Workers[id].tasks.pop())) {
        task->executor->execute();
        continue;
      }

      // Steal from one of the others...      
      if ((task = Workers[Math::randint(0, maxId)].tasks.steal())) {
        task->executor->execute();
      }
    }
  }
};

}} // namespace Voxx::Conky