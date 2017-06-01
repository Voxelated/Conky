//==--- tests/ContainerBenchmarks.cpp ---------------------- -*- C++ -*- ---==//
//            
//                                    Voxel
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//  
//==------------------------------------------------------------------------==//
//
/// \file  ContainerBenchmarks.cpp
/// \brief This file defines benchmarks for containers.
// 
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Concurrent/StaticStealableQueue.hpp>
#include <Voxel/Conky/Container/Task.hpp>
#include <Voxel/Conky/Thread/Thread.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <benchmark/benchmark.h>
#include <thread>

// Tests the performance of pusing onto the queue.
template <typename ObjType, std::size_t Size>
static void BmPush(benchmark::State& state, int elements) {
  using namespace Voxx;
  using QueueType = Conky::StaticStealableQueue<ObjType, Size>;
  Thread::setAffinity(state.thread_index % System::CpuInfo::cores());

  QueueType queue;
  while (state.KeepRunning()) {
    for (int i = 0; i < elements; ++i)
      queue.push(ObjType());
  }
}

// Tests the performance of having one thread push and the others steal. I.e
// with 2 threads, the queue acts as an SPSC queue, for 3 threads its SPMC with
// 2 consumers, and with 4 threads is SPMC with 3 consumers, etc.
template <typename ObjType, std::size_t Size>
static void BmPushSteal(benchmark::State& state, int elements) {
  using namespace Voxx;
  using QueueType = Conky::StaticStealableQueue<ObjType, Size>;
  Thread::setAffinity(state.thread_index % System::CpuInfo::cores());

  static QueueType queue;
  static int       barrier;
  if (state.thread_index != 0) {
    while (barrier != state.threads) {};
  } else {
    barrier = state.threads;
  }

  while (state.KeepRunning()) {
    if (state.thread_index == 0) {
      barrier = state.threads;
      for (int i = 0; i < elements; ++i)
        queue.push(ObjType());
    } else if (state.thread_index >= 1) {
      while (queue.size() && barrier != -1) {
        if (auto res = queue.steal()) {
          benchmark::DoNotOptimize(*res);
        }
      }
    }

    // Tell workers to stop:
    if (state.thread_index == 0) {
      barrier = -1;
    }
  }
}

// Tests the performance of having one thread push and the others steal, when
// the container stores tasks.
template <std::size_t Size>
static void BmPushStealTask(benchmark::State& state, int elements) {
  using namespace Voxx;
  using TaskType  = Conky::Task<64>;
  using QueueType = Conky::StaticStealableQueue<TaskType, Size>;
  Thread::setAffinity(state.thread_index % System::CpuInfo::cores());

  static QueueType        queue;
  static std::atomic<int> barrier = 0;
  if (state.thread_index != 0) {
    while (barrier != state.threads) {}
  } else {
    barrier = state.threads;
  }

  while (state.KeepRunning()) {
    if (state.thread_index == 0) {
      for (int i = 0; i < elements; ++i) {
        queue.push([] {
          int res = 0;
          benchmark::DoNotOptimize(res++);
        });
      }
    } else if (state.thread_index >= 1) {
      while (queue.size() && barrier.load(std::memory_order_relaxed) != -1) {
        if (auto res = queue.steal()) {
          res->executor->execute();
        }
      }
    }

    // Tell workers to stop:
    if (state.thread_index == 0) {
      barrier = -1;
    }
  }
}


// This provides a reference for the task versus if the task was inlined.
static void BmTaskInlineReference(benchmark::State& state, int elements) {
  int x;
  while (state.KeepRunning()) {
    for (int i = 0; i < elements; ++i) {
      benchmark::DoNotOptimize(x++);
    }
  }
}

// This tests the performance of calling a task.
template <std::size_t TaskSize>
static void BmTaskInvoke(benchmark::State& state, int elements) {
  using TaskType = Voxx::Conky::Task<TaskSize>;
  TaskType task([]  {
    int x;
    benchmark::DoNotOptimize(x++);
  });

  while (state.KeepRunning()) {
    for (int i = 0; i < elements; ++i) {
      task.executor->execute();
    }
  }
}

//==--- Constants ----------------------------------------------------------==//

static constexpr int         testElements = 1000000;
static constexpr std::size_t queueSize    = 1 << 20;

//==------------------------------------------------------------------------==//

int main(int argc, char** argv) {
  using namespace benchmark;
  Voxx::System::CpuInfo::refresh();

  RegisterBenchmark(
    "StealableQueuePush", BmPush<int, queueSize>, testElements)
    ->UseRealTime()->Threads(1);
  RegisterBenchmark(
    "StealableQueuePushSteal", BmPushSteal<int, queueSize>, testElements)
    ->UseRealTime()->DenseThreadRange(1, 4, 1);
  RegisterBenchmark(
    "StealableQueuePushStealTask", BmPushStealTask<queueSize>, testElements)
    ->UseRealTime()->Threads(4);
  RegisterBenchmark(
    "TaskReference", BmTaskInlineReference, testElements)
    ->UseRealTime()->Threads(1);
  RegisterBenchmark(
    "TaskInvoke64Byte", BmTaskInvoke<64>, testElements)
    ->UseRealTime()->Threads(1);
  RegisterBenchmark(
    "TaskInvoke128Byte", BmTaskInvoke<128>, testElements)
    ->UseRealTime()->Threads(1);

  Initialize(&argc, argv);
  RunSpecifiedBenchmarks();
}