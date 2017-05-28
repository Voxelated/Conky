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
#include <benchmark/benchmark.h>
#include <thread>
#include <unordered_map>
#include <experimental/random>

/*
template <typename T>
struct Handler {
  void operator()(const T& object) noexcept {
  }
};

using IntThreadPool =
  Voxx::Conky::ThreadPool<unsigned int, Handler<unsigned int>, 1048574>;

// The throughput test is to test the ability of the thread pool to process
// elements.
static void
benchmarkThreadPoolSPSCThroughput(benchmark::State& state, unsigned int iters) {
  Voxx::System::CpuInfo::refresh();

  // Create a 2-thread thread pool : 1 processing thread, one submission thread.
  IntThreadPool threadPool(2);
  while (state.KeepRunning()) {
    for (unsigned int i = 0; i < iters; ++i) {
      threadPool.push(i);
    }
  }
}
*/

static void benchmarkRandGenVoxel(benchmark::State& state) {
  uint32_t r = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(r = Voxx::Math::randint(0, 10));
  }
}

static void benchmarkRandGenStd(benchmark::State& state) {
  int r = 0;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(r = std::experimental::randint(0, 10));
  }
}

// This tests the performance of querying a thread id from a vector of thread
// ids.
static void benchmarkThreadIdVector(benchmark::State& state) {
  std::vector<std::thread::id> ids{
    std::thread::id(), std::thread::id(),
    std::thread::id(), std::thread::id(),
    std::thread::id(), std::thread::id(),
    std::thread::id(), std::thread::id(),
    std::this_thread::get_id()};

  while (state.KeepRunning()) {
    auto id = std::this_thread::get_id();
    for (std::size_t i = 0; i < ids.size(); ++i) {
      if (ids[i] == id)
        break;
    }
  }
}

// This tests the performance of using a thread local thread id.
static thread_local std::size_t threadid = 0;
static void benchmarkThreadIdThreadLocal(benchmark::State& state) {
  std::size_t v = 0;
  while (state.KeepRunning())
    benchmark::DoNotOptimize(v = threadid);

}

// This tests the performance of using a hash for storing a thread id.
static void benchmarkThreadIdHash(benchmark::State& state) {
  std::unordered_map<std::thread::id, std::size_t> ids;
  ids[std::this_thread::get_id()] = 4;

  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(ids[std::this_thread::get_id()]);
  }
}

//==--- Thread related benchmarks ------------------------------------------==//

BENCHMARK(benchmarkThreadIdVector);
BENCHMARK(benchmarkThreadIdThreadLocal);
BENCHMARK(benchmarkThreadIdHash);
BENCHMARK(benchmarkRandGenVoxel);
BENCHMARK(benchmarkRandGenStd);
//BENCHMARK_CAPTURE(benchmarkThreadPoolSPSCThroughput, intThreadPool, 65000);

//==------------------------------------------------------------------------==//

BENCHMARK_MAIN();