//==--- tests/TaskBenchmarks.cpp --------------------------- -*- C++ -*- ---==//
//            
//                                    Voxel
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//  
//==------------------------------------------------------------------------==//
//
/// \file  TaskBenchmarks.cpp
/// \brief This file defines benchmarks for tasks.
// 
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Container/Task.hpp>
#include <benchmark/benchmark.h>

// The number of iterations for each of the tests.
static constexpr int iterations = 50000;

__attribute__((always_inline)) void f(int a, int b) {
  benchmark::DoNotOptimize(a += b); 
}

// Benchmarks the cost of executing a lambda, for reference against a task.
static void benchmarkStatelessLambdaRef(benchmark::State& state, int iters) {
  //auto f = [] (int a, int b) { benchmark::DoNotOptimize(a += b); };
  while (state.KeepRunning())
    for (int i = 0; i < iters; ++i)
      f(1, 2);
}

// Benchmarks the cost of executing a lambda, for reference against a task.
static void benchmarkStatefullLambdaRef(benchmark::State& state, int iters) {
  int a = 1, b = 2;
  auto f = [&a, &b] { benchmark::DoNotOptimize(a += b); };
  while (state.KeepRunning())
    for (int i = 0; i < iters; ++i)
      f();
}

// Benchmarks the cost of a callable object.
static void benchmarkCallableRef(benchmark::State& state, int iters) {
  auto callable = Voxx::Function::makeCallable(
    [] (int a, int b) { benchmark::DoNotOptimize(a += b); }, 1, 2
  );

  while (state.KeepRunning())
    for (int i = 0; i < iters; ++i)
      callable();
}

// Benchmarks the cost of executing a stateless task via argument passing.
static void benchmarkStatelessArgPassTask(benchmark::State& state, int iters) {
  using Task = Voxx::Conky::Task<64>;  

  Task task([] (int a, int b) { benchmark::DoNotOptimize(a += b); }, 1, 2);

  while (state.KeepRunning()) {
    for (int i = 0; i < iters; ++i)
      task.executor->execute();
  }
}

// Benchmarks the cost of executing a statelss task via argument copy capture.
static void
benchmarkStatelessArgCaptureTask(benchmark::State& state, int iters) {
  using Task = Voxx::Conky::Task<64>;  

  int a = 1, b = 2;
  Task task([a, b] () mutable { benchmark::DoNotOptimize(a += b); });
  while (state.KeepRunning()) {
    for (int i = 0; i < iters; ++i)
      task.executor->execute();
  }
}

// Benchmarks the cost of executing a statefull task via argument ref capture.
static void
benchmarkStatefullArgCaptureTask(benchmark::State& state, int iters) {
  using Task = Voxx::Conky::Task<64>;  

  int a = 1, b = 2;
  Task task([&a, &b] { benchmark::DoNotOptimize(a += b); });
  while (state.KeepRunning()) {
    for (int i = 0; i < iters; ++i)
      task.executor->execute();
  }
}

//==--- Register benchmarks ------------------------------------------------==//

BENCHMARK_CAPTURE(benchmarkStatelessLambdaRef     , iters, iterations);
BENCHMARK_CAPTURE(benchmarkStatefullLambdaRef     , iters, iterations);
BENCHMARK_CAPTURE(benchmarkCallableRef            , iters, iterations);
BENCHMARK_CAPTURE(benchmarkStatelessArgPassTask   , iters, iterations);
BENCHMARK_CAPTURE(benchmarkStatelessArgCaptureTask, iters, iterations);
BENCHMARK_CAPTURE(benchmarkStatefullArgCaptureTask, iters, iterations);

//==------------------------------------------------------------------------==//

BENCHMARK_MAIN();