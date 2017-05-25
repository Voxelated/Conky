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
/// \brief This file provides the definition of a thread pool.
//
//==------------------------------------------------------------------------==//

#pragma once

#include "ThreadPoolException.hpp"
#include <Voxel/Conky/Concurrent/CircularDequeue.hpp>
#include <Voxel/Conky/Thread/Thread.hpp>
#include <vector>

namespace Voxx  {
namespace Conky {

/// The ThreadPool class stores per thread circular dequeues of some objects.
/// The objects could be anything: callable's, data, etc.
/// \tparam Object  The type of object to store in the per thread object pools.
/// \tparam Handler The handler for when objects are taken from the queue.
template <typename Object, typename Handler, std::size_t QueueSize = 2048>
class ThreadPool {
 private:
  //==--- Constants --------------------------------------------------------==//
  
  /// Defines the size of the required alignment to to avoid false sharing.
  static constexpr std::size_t nonDestructiveSize =
    std::hardware_destructive_interface_size;

  /// The Worker struct contains a queue of objects for the worker to work on,
  /// and a flag which allows it to be interrupted/suspended, etc. The alignment
  /// of the struct ensures that there is no false sharing between workers.
  alignas(nonDestructiveSize) struct Worker {
    //==--- Aliases --------------------------------------------------------==//
    
    /// Alias for the type of the object queue.
    using ObjectQueue = CircularDequeue<Object, QueueSize>; 
    /// Alias for the type of the flag used for controlling the worker.
    using FlagType    = std::atomic<bool>;

    //==--- Members --------------------------------------------------------==//
    
    ObjectQueue objects;          //!< The queue of objects for the worker.
    FlagType    runnable = false; //!< A flag to control the worker.
  };

 public:
  //==--- Aliases ----------------------------------------------------------==//

  /// Defines the type of container used to store each of the object pools.
  using WorkerPool      = std::vector<Worker>;
  /// Defines the type of thread container.
  using ThreadContainer = std::vector<Threads>;
  /// Defines the type of the handler to use when extracting objects from
  /// thread local object pools.
  using HandlerType     = Handler;

  //==--- Con-destruction --------------------------------------------------==//
  
  /// Constructor -- creates the threads, and starts them running.
  /// \param[in] threadCount  The number of threads to create.
  ThreadPool(std::size_t numThreads, HandlerType&& hander = HandlerType())
  : Workers(numThreads) {
    // \todo Add warning if numThreads > physical cores.
    // Debug::catcher([numThreads] () {
    //  if (numThreads > System::CpuInfo::cores())
    //    throw ThreadPoolException(
    //      ThreadPoolException::Type::Oversubscription);
    // });
    createThreads(numThreads);
    runAllWorkers();
  }

  /// Destructor -- joins the threads.
  ~ThreadPool() noexcept {
    shutdown();
  }

  //==--- Methods ----------------------------------------------------------==//
  
  /// Pushes an object onto the worker queue for the current thread.
  /// \tparam object The object to push onto the worker queue.
  void push(const Object& object) {
    // // Spin while full ..
    while (workers[Thread::threadId].size() == QueueSize) {}
    workers[Thread::threadId].push(object);
  }

  /// Moves an object onto the worker queue for the current thread.
  /// \tparam object The object to move onto the worker queue.
  void push(Object object) {
    // Spin while full ..
    while (workers[Thread::threadId].size() == QueueSize) {}
    workers[Thread::threadId].push(std::move(object));
  }
  
  /// Stops the threads. Currently this just joins them, but really this should
  /// interrupt the threads if they are running.
  void shutdown() noexcept {
    for (auto& worker : Workers)      
      worker.runnable.store(false, std::memory_order_release);

    for (auto& thread : Threads)
      thread.join();
  }

 private:
  ThreadContainer Threads;       //!< The threads for the pool.
  WorkerPool      Workers;       //!< Per thread pools of objects.
  HandlerType     ObjectHandler; //!< Handler to use on extracted objects.
                                            
  /// Creates the threads.
  void createThreads(std::size_t numThreads) {
    // TODO: Change this to get a proper thread index.
    for (std::size_t i = 0; i < numThreads; ++i)
      Threads.push_back(std::thread(&ThreadPool::process, this, i));
  }

  /// Sets all threads to runnable.
  void runAllWorkers() {
    for (const auto& worker : Workers)
      worker.runnable.store(true, std::memory_order_relaxed);
  }

  /// Processes elements using the \p threadId thread and the \p threadId worker.
  /// \param[in] threadIndex The index of the thread and worker, and the global
  ///                        index of the thread.
  void process(std::size_t threadIndex) {
    Thread::setAffinity(threadIndex);

    // The global threadId is set to the thread index so that when pushing to
    // the thread pool, the correct worker can be accessed extremely quickly.
    Thread::threadId = threadIndex;
    auto& worker     = Worker[threadIndex];

    while (worker.runnable.load(std::memory_order_relaxed)) {
      if (auto object = worker.objects.pop()) {
        ObjectHandler(std::move(*object));
      } else {
        // Need to steal from one of the other workers.
        
      }
    }
  }
};

}} // namespace Voxx::Conky