//==--- Conky/Concurrent/ThreadPool.hpp -------------------- -*- C++ -*- ---==//
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

#include "CircularDequeue.hpp"
#include <vector>

namespace Voxx  {
namespace Conky {

/// The ThreadPoolException class overrides the standard exception class to
/// define thread pool related exceptions.
class ThreadPoolException : public std::exception {
 public:
  /// The Type enum defines the types of thread pool realted exceptions.
  enum class Type : uint8_t {
    Oversubscription = 0x00  //!< More threads than cores.
  };

 public:
  /// Constructor -- initializes the type of the thread error.
  /// \param[in]  type          The type of the thread error.
  /// \param[in]  threadNumber  The number of the thread with the error.
  ThreadPoolException(Type type) noexcept : ErrorType(type) {}

  /// Display a message for the exception.
  virtual const char* what() const noexcept override {
    const std::string msg = std::string("\nThreadPool Exception:\n\t")
                          + message();
    return msg.c_str();
  }

 private:
  Type ErrorType; //!< The type of the thread pool exception.
                  
  /// Returns char string pointer with an appropriate message for the type of
  /// the error.
  const char* message() const noexcept;
};

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
    //  throw ThreadException(ThreadException::Type::Oversubscription);
    // });
    // if (numThreads > System::CpuInfo::cores())
    //    
    createThreads();
    runAllWorkers();
  }

  /// Destructor -- joins the threads.
  ~ThreadPool() noexcept {
    shutdown();
  }

  //==--- Methods ----------------------------------------------------------==//
  
  /// Stops the threads. Currently this just joins them, but really this should
  /// interrupt the threads if they are running.
  void shutdown() noexcept {
    for (auto& worker : Workers)      
      worker.runnable.store(false, std::memory_order_release);

    for (auto& thread : Threads)
      thread.join();
  }

 private:
  ThreadContainer  Threads;       //!< The threads for the pool.
  WorkerPool       Workers;       //!< Per thread pools of objects.
  HandlerType      ObjectHandler; //!< Handler to use on extracted objects.
                                            
  /// Creates the threads.
  void createThreads() {
    // TODO: Change this to get a proper thread index.
    for (std::size_t i = 0; i < threadCount; ++i)
      Threads.push_back(std::thread(&ThreadPool::run, this, i));
  }

  /// Sets all threads to runnable.
  void runAllWorkers() {
    for (const auto& worker : Workers)
      worker.runnable.store(true, std::memory_order_relaxed);
  }

  /// Processes elements using the \p threadId thread and the \p threadId worker.
  /// \param[in] threadId The index of the thread and worker to process for.
  void process(std::size_t threadId) {
    auto& worker = Worker[threadIndex];
    while (worker.runnable.load(std::memory_order_relaxed)) {
      if (auto object = worker.objects.pop()) {
        ObjectHandler(std::move(&object));
      } else {
        // Need to steal from one of the other workers.
        
      }
    }
  }
};

}} // namespace Voxx::Conky