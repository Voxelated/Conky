//==--- tests/ThreadPoolBenchmarks.cpp --------------------- -*- C++ -*- ---==//
//            
//                                    Voxel
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//  
//==------------------------------------------------------------------------==//
//
/// \file  ThreadPoolBenchmarks.cpp
/// \brief This file defines benchmarks for the thread pool.
// 
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Container/ThreadPool.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <Voxel/Thread/Thread.hpp>
#include <benchmark/benchmark.h>

using namespace Voxx;
using namespace Voxx::Conky;

//==--- Constants ----------------------------------------------------------==//

static constexpr std::size_t poolElements = 1000000;
static constexpr std::size_t poolSize     = 1 << 20;

//==--- Benchmarks ---------------------------------------------------------==//

// This tests the performance of pushing onto one thread while the other threads
// steal. This can show the performance of the stealing policies under heavy
// contention.
// 
// There are a total of poolElements tasks pushed onto the pool, and therefore
// poolElements tasks executed by the N threads.
// 
// Thre reported rates are the times for all threads to complete the accumlated
// work.
template <StealPolicy Steal>
static void BmThreadPoolProduceConsume(benchmark::State& state) {
  using namespace std::chrono;
  using namespace benchmark;

  ThreadPool<poolSize, Steal> threadPool(System::CpuInfo::cores());
  const double totalTasks = poolElements;

  while (state.KeepRunning()) {
    auto start = high_resolution_clock::now();
    for (std::size_t i = 0; i < poolElements; ++i) {
      auto pushed = threadPool.tryPush([] {
        int x = 0;
        DoNotOptimize(x++);
      });
    }
    threadPool.waitTillIdle();

    auto end      = high_resolution_clock::now();
    auto seconds  = duration_cast<duration<double>>(end - start).count();
    auto nseconds = duration_cast<nanoseconds>(end - start).count();
    state.SetIterationTime(seconds);

    state.counters.insert({
      {"Time/task (ns)", static_cast<double>(nseconds) / totalTasks},
      {"Tasks/sec (M)" , (totalTasks / 1000000.0)  / seconds}       ,
      {"Threads"       , System::CpuInfo::cores()}
    });
  }
}

// This tests the performance of each thread pushing onto it's own queue
// and then timing the performance of emptying the queue. This is intended
// to show the performance when work is well distributed, i.e when threads are
// creating work for themselves, such that there is minimal stealing and
// therefore minimal contention.
// 
// There are a total of poolElements * N tasks pushed onto the pool, and
// therefore poolElements * N tasks executed by the N threads.
// 
// Thre reported rates are the times for all threads to complete the accumlated
// work, which is poolElements * N (i.e 'work' is the action of generating and
// executing a task).
template <StealPolicy Steal>
static void BmThreadPoolProduceConsumeOwn(benchmark::State& state) {
  using namespace std::chrono;
  using namespace benchmark;

  ThreadPool<poolSize, Steal> threadPool(System::CpuInfo::cores());
  const double totalTasks = poolElements * (System::CpuInfo::cores() - 1);
  std::atomic<int> barrier = 0;

  while (state.KeepRunning()) {
    for (std::size_t i = 1; i < System::CpuInfo::cores(); ++i) {
      threadPool.tryPush([&threadPool, &barrier] {
        // Wait till everyone is ready:
        barrier.fetch_add(1, std::memory_order_relaxed);
        while (barrier.load() < System::CpuInfo::cores()) { /* Spin ... */}

        // Fill the worker's task queue.
        for (std::size_t i = 0; i < poolElements; ++i) {
          auto pushed = threadPool.tryPush([] {
            int x = 0;
            DoNotOptimize(x++);
          });
        }
      });
    }

    // Time once all threads are at the same point:
    barrier.fetch_add(1);
    while (barrier.load() < System::CpuInfo::cores()) { /* Spin ... */ }
    auto start = high_resolution_clock::now();

    // Wait for threads to finish:
    threadPool.waitTillIdle();

    auto end      = high_resolution_clock::now();
    barrier.store(0);

    auto seconds  = duration_cast<duration<double>>(end - start).count();
    auto nseconds = duration_cast<nanoseconds>(end - start).count();
    state.SetIterationTime(seconds);

    state.counters.insert({
      {"Time/task (ns)", static_cast<double>(nseconds) / totalTasks},
      {"Tasks/sec (M)" , (totalTasks / 1000000.0)  / seconds}       ,
      {"Threads"       , System::CpuInfo::cores() - 1}
    });
  }
}

// This has the main thread pish a task onto it's queue, and when a worker
// steals the task, it creates a task on its own queue, which will execute, and
// then try and steal from the main queue again.
// 
// There are thus a total of cores * poolElements tasks enqued (across all
// workers) and cores * poolElements tasks dequeued and executed.
// 
// The reported rates are the the combined averages to push all the tasks and
// execute them, so the result is the number of tasks which can be pushed and
// executed in a second.
template <StealPolicy Steal>
static void BmThreadPoolPushOnSteal(benchmark::State& state) {
  using namespace std::chrono;
  using namespace benchmark;

  ThreadPool<poolElements, Steal> threadPool(System::CpuInfo::cores());
  const double totalTasks = System::CpuInfo::cores() * poolElements;

  while (state.KeepRunning()) {
    auto start = high_resolution_clock::now();
    for (std::size_t i = 0; i < poolElements; ++i) {
      auto pushed = threadPool.tryPush([&threadPool, i] {
        threadPool.tryPush([i] {
          int x = 0;
          DoNotOptimize(x++);
        });
      });
    }
    threadPool.waitTillIdle();

    auto end      = high_resolution_clock::now();
    auto seconds  = duration_cast<duration<double>>(end - start).count();
    auto nseconds = duration_cast<nanoseconds>(end - start).count();
    state.SetIterationTime(seconds);

    state.counters.insert({
      {"Time/task (ns)", static_cast<double>(nseconds) / totalTasks},
      {"Tasks/sec (M)" , totalTasks / seconds / 1000000.0}          ,
      {"Threads"       , System::CpuInfo::cores()}
    });
  }
}

/// This tests the amount of time required for a single thread to enqueue tasks
/// while no workers are running.
static void BmThreadPoolEnqueueOnly(benchmark::State& state) {
  using namespace std::chrono;
  using namespace benchmark;

  ThreadPool<poolElements> threadPool(System::CpuInfo::cores());
  const double totalTasks = poolElements;
  threadPool.stopThreads();

  while (state.KeepRunning()) {
    auto start = high_resolution_clock::now();
    for (std::size_t i = 0; i < poolElements; ++i) {
      auto pushed = threadPool.tryPush([] {
        int x = 0;
        benchmark::DoNotOptimize(x++);
      });
    }

    auto end      = high_resolution_clock::now();
    auto seconds  = duration_cast<duration<double>>(end - start).count();
    auto nseconds = duration_cast<nanoseconds>(end - start).count();
    state.SetIterationTime(seconds);

    state.counters.insert({
      {"Time/task (ns)", static_cast<double>(nseconds) / totalTasks},
      {"Tasks/sec (M)" , totalTasks / seconds / 1000000.0}          ,
      {"Threads"       , 1}
    });
  }
}

// This tests the amount of time required to empty the thread pool.
template <StealPolicy Policy>
static void BmThreadPoolEmptyTest(benchmark::State& state) {
  using namespace std::chrono;
  using namespace benchmark;

  ThreadPool<poolElements, Policy> threadPool(System::CpuInfo::cores());
  const double totalTasks = poolElements;

  while (state.KeepRunning()) {
    threadPool.stopThreads();
    for (std::size_t i = 0; i < poolElements; ++i) {
      auto pushed = threadPool.tryPush([] {
        int x = 0;
        DoNotOptimize(x++);
      });
    }
    auto start = high_resolution_clock::now();

    threadPool.startThreads();
    threadPool.waitTillIdle();

    auto end      = high_resolution_clock::now();
    auto seconds  = duration_cast<duration<double>>(end - start).count();
    auto nseconds = duration_cast<nanoseconds>(end - start).count();
    state.SetIterationTime(seconds);

    state.counters.insert({
      {"Time/task (ns)", static_cast<double>(nseconds) / totalTasks},
      {"Tasks/sec (M)" , totalTasks / seconds / 1000000.0}          ,
      {"Threads"       , System::CpuInfo::cores() - 1}
    }); 
  }
}

// This tests the amount of time required to empty the thread pool, where each
// thread's work pool is pre filled, and then each worker empties its own queue.
// Early finishing workers will start stealing from other queues.
static void BmThreadPoolEmptyTestOwnQueue(benchmark::State& state) {
  using namespace std::chrono;
  using namespace benchmark;

  ThreadPool<poolElements> threadPool(System::CpuInfo::cores());
  const double totalTasks = poolElements * System::CpuInfo::cores() - 1;

  std::atomic<int> barrier = 0;
  while (state.KeepRunning()) {
    for (std::size_t i = 1; i < System::CpuInfo::cores(); ++i) {
      threadPool.tryPush([&barrier, &threadPool] {
        // Fill the worker's task queue.
        for (std::size_t i = 0; i < poolElements; ++i) {
          auto pushed = threadPool.tryPush([] {
            int x = 0;
            DoNotOptimize(x++);
          });
        }

        // Wait till all are full, then each worker will empty its own queue.
        barrier.fetch_add(1);
        while (barrier.load() < System::CpuInfo::cores()) { /* Spin ... */}
      });
    }
    barrier.fetch_add(1);
    while (barrier.load(std::memory_order_relaxed) < System::CpuInfo::cores()) {
      // Spin ...
    }

    auto start = high_resolution_clock::now();
    threadPool.waitTillIdle();
    auto end      = high_resolution_clock::now();
    
    auto seconds  = duration_cast<duration<double>>(end - start).count();
    auto nseconds = duration_cast<nanoseconds>(end - start).count();
    state.SetIterationTime(seconds);

    state.counters.insert({
      {"Time/task (ns)", static_cast<double>(nseconds) / totalTasks},
      {"Tasks/sec (M)" , totalTasks / seconds / 1000000.0}          ,
      {"Threads"       , System::CpuInfo::cores() - 1    }
    }); 
  }
}

//==------------------------------------------------------------------------==//

int main(int argc, char** argv) {
  using namespace benchmark;
  Voxx::System::CpuInfo::refresh();

  //RegisterBenchmark("ThreadPoolStartStop", BmThreadPoolStartStop, defaultIters);
  RegisterBenchmark("ThreadPoolProduceOneStealRestRandom", 
                    BmThreadPoolProduceConsume<StealPolicy::Random>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolProduceOneStealRestNNeighbour", 
                    BmThreadPoolProduceConsume<StealPolicy::NearestNeighbour>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolPushAllPopAllRandom", 
                    BmThreadPoolProduceConsumeOwn<StealPolicy::Random>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolPushAllPopAllNNeighbour", 
                    BmThreadPoolProduceConsumeOwn<StealPolicy::NearestNeighbour>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolPushOnStealRandom",
                    BmThreadPoolPushOnSteal<StealPolicy::Random>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolPushOnStealNNeighbour",
                    BmThreadPoolPushOnSteal<StealPolicy::NearestNeighbour>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolEnqueueOnlySingeThread",
                    BmThreadPoolEnqueueOnly)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolHeavyConcurrentEmptyRandom",
                    BmThreadPoolEmptyTest<StealPolicy::Random>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolHeavyConcurrentEmptyNNeighbour",
                    BmThreadPoolEmptyTest<StealPolicy::NearestNeighbour>)
                    ->UseRealTime()->Threads(1);
  RegisterBenchmark("ThreadPoolOwnQueueEmpty",
                    BmThreadPoolEmptyTestOwnQueue)
                    ->UseRealTime()->Threads(1);
  Initialize(&argc, argv);
  RunSpecifiedBenchmarks();
}
