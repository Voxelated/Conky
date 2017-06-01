//==--- tests/ContainerTests.hpp --------------------------- -*- C++ -*- ---==//
//            
//                                    Voxel
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//  
//==------------------------------------------------------------------------==//
//
/// \file  ContainerTests.cpp
/// \brief This file defines tests for containers.
// 
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Concurrent/StaticStealableQueue.hpp>
#include <Voxel/Conky/Container/Task.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <Voxel/Thread/Thread.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// This barrier is used to start threads at a similar time.
std::atomic<int> barrier = 0;
std::mutex       exclusive;

/// Fixture class for work stealing queue tests.
class StaticStealableQueueTest : public ::testing::Test {
 protected:
  /// Defines the number of elements in the dequeue.
  static constexpr std::size_t queueSize    = 1 << 13;
  /// Defines the number of element to push onto the queue.
  static constexpr std::size_t testElements = queueSize << 4;

  /// Alias for the type of data in the queue.
  using DataType   = int;
  /// Alias for the type of the queue.
  using QueueType  = Voxx::Conky::StaticStealableQueue<DataType, queueSize>;
  /// Alias for the type of the restults container.
  using ResultType = std::vector<DataType>;

  /// This constantly tries to steal elements from the queue, and stores any
  /// stolen results in the thread's results vector.
  /// \param[in] threadIdx    The index of the thread.
  /// \param[in] totalThreads The total number of threads used for the test.
  void steal(int threadIdx, int totalThreads) {
    Voxx::Thread::setAffinity(threadIdx);
    thread_local auto threadResults = ResultType{};
    barrier.fetch_add(1);

    // Spin while we wait for other theads:
    while (barrier.load() < totalThreads) {}

    // Run until the main thread says stop:
    while (barrier.load() != 0) {
      // This loop is to avoid contention on the atomic:
      for (std::size_t i = 0; i < testElements; ++i)
        if (auto result = queue.steal()) {
          threadResults.push_back(*result);
        }
    }

    std::lock_guard<std::mutex> guard(resultMutex);
    results[threadIdx] = threadResults;
  }

  /// This constantly pushes elements onto the queue, occasionally popping some.
  /// \param[in] threadIdx      The index of the thread.
  /// \param[in] totalThreads   The total number of threads running.
  void pushAndPop(int threadIdx, int totalThreads) {
    Voxx::Thread::setAffinity(threadIdx);
    thread_local auto threadResults = ResultType{};
    barrier.fetch_add(1);

    // Wait until all threads are ready:
    while (barrier.load() < totalThreads) {}

    // Push and occasionally pop. Since threads are stealing, there should be a
    // lot of contention on the first element in the queue.
    std::size_t notPushed = 0;
    for (size_t i = 0; i < testElements; ++i) {
      while (!queue.tryPush(i)) { /* Spin, queue full */ }
      if (i & 1ull)
        if (auto result = queue.pop())
          threadResults.push_back(*result);
    }

    // We don't need the lock here because the strealing threads can only
    // execute their writes to the global results container when this thread
    // tells them to stop stealing.
    results[threadIdx] = threadResults;

    // Tell the other threads to stop:
    barrier.store(0);
  }

  /// Generates \p elements elements for the queue.
  /// \param[in] elements   The number of elements to generate.
  void generate(std::size_t elements) {
    while (elements-- > 0)
      queue.push(elements);
  }

  /// Starts the threads by setting the first thread to push and pop, and
  /// setting the other threads to constantly steal from the queue.
  void run() {
    const std::size_t cores = Voxx::System::CpuInfo::cores();
    threads.emplace_back(&StaticStealableQueueTest::pushAndPop, this, 0, cores);
    runStealers();
  }

  /// Runs only stealing threads.
  void runStealers() {
    const std::size_t cores = Voxx::System::CpuInfo::cores();
    for (std::size_t i = 1; i < cores; ++i)
      threads.emplace_back(&StaticStealableQueueTest::steal, this, i, cores);
  }

  /// Joins the threads.
  void join() {
    barrier.store(0);
    for (auto& thread : threads)
      thread.join();
  }

  /// Set's up the results containers.
  void setUp() {
    for (std::size_t i = 0; i < Voxx::System::CpuInfo::cores(); ++i)
      results.push_back(ResultType());
  }

  QueueType                queue;       //!< The queue to test.
  std::vector<ResultType>  results;     //!< Vectors of results for each thread.
  std::vector<std::thread> threads;     //!< Thread to test the queue with.
  std::mutex               resultMutex; //!< Mutex for pushing results.
};

TEST_F(StaticStealableQueueTest, CorrectlyDeterminesSize) {
  generate(queueSize >> 1);
  EXPECT_EQ(queue.size(), (queueSize >> 1));
}

TEST_F(StaticStealableQueueTest, CanPopSingleThreaded) {
  generate(queueSize);
  for (std::size_t i = queueSize; i > 0; --i) {
    EXPECT_EQ(queue.size(), i);
    EXPECT_TRUE(queue.pop());
  }
  EXPECT_EQ(queue.size(), 0);

  for (std::size_t i = 0; i < (queueSize >> 4); ++i) {
    EXPECT_FALSE(queue.pop());
    EXPECT_EQ(queue.size(), 0);
  }
}

// This test is designed to test if the StaticStealableQueue breaks. There are
// two cases which "break" the deque:
//
// 1. Pushing onto the queue when it is full: This is defined in the queue's
//    API, as it would require an extra check on each push to the dequeue.
//    The API requires that the user simply specify a large enough dequeue,
//    which should neven be a problem on current hardware. DynamicDequeue may be
//    added which can be resized.
//
// 2. Concurrent popping and stealing: If there is a single element in the queue
//    then the thread owning the deque (the one which may push), will be in a
//    race with the other threads (which are trying to steal) for the last 
//    element. An incorrect implementation would allow pops and steals to
//    access the same element.
//
// To test case 2, this test has the first thread pusing to the queue, and
// occasionally popping, while other threads steal. As there will be more
// threads stealing, as well as the one thread having to push and pop, the
// queue will not grow and there will be constant contention to get the first
// element.
//
// Each thread stores the results of popped or stolen items, and if no item
// appears in multiple thread's results, then there has not been any error.
// 
// While this test only runs a relatively small number of elements, the
// imlementation has been tested on large queue sizes (1 << 29), run multiple
// times (approximately 1 day of continuous contention on the last element) and
// ther were no data races.
TEST_F(StaticStealableQueueTest, PopAndStealRaceCorrectly) {
  setUp();
  run();
  join();

  // Check that each item was only taken off the queue once:
  std::vector<size_t> counters(results.size(), 0);
  std::unordered_map<std::size_t, std::size_t> resultMap;
  for (std::size_t i = 0; i < results.size(); ++i) {
    for (const auto& result : results[i]) {
      auto search = resultMap.find(result);
      if (search != resultMap.end()) {
        printf("Duplicate: %12i, threads: %2lu,%2lu\n", 
               result, i, search->second);
        EXPECT_TRUE(false);
      }
      resultMap.insert({result, i});
    }
  }

  for (std::size_t element = 0; element < testElements; ++element) {
    auto found = resultMap.find(element);
    if (found == resultMap.end()) {
      printf("Element %12lu was not found\n", element);
      EXPECT_TRUE(false);
    }
  }
}

// This test uses tryPush, and should result in each element being found in a
// result vector. If push is used here instead of tryPush, it's possible that
// the queue would have overflowed before one of the workers was able to steal
// the first elements pushed, and therefore some of the early elements wont
// be found.
TEST_F(StaticStealableQueueTest, TryPushIsSafe) {
  Voxx::Thread::setAffinity(0);
  barrier.store(0);

  setUp();
  runStealers();

  barrier.fetch_add(1);
  for (std::size_t i = 0; i < queueSize * 2; ++i)
    while (!queue.tryPush(static_cast<DataType>(i))) {}

  while (queue.size()) {}
  join();

  // Check that every element is in a result vector.
  std::vector<size_t> counters(results.size(), 0);
  for (std::size_t element = 0; element < queueSize * 2; ++element) {
    bool found = false;
    for (std::size_t j = 0; j < results.size(); ++j) {
      if (results[j].empty())
        continue;

      auto& index     = counters[j];
      auto& container = results[j];
      if (container[index] == element) {
        found = true;
        index++;
      }
    }
    if (!found) {
      printf("Element %12lu was not found\n", element);
      ASSERT_TRUE(false);
    }
    EXPECT_TRUE(found);
  }
}

TEST(TaskTests, CanCreateGenericTasks) {
  using Task = Voxx::Conky::Task<64>;
  std::vector<double> vec;

  Task taska([] (int a, float b) {}, 4, 3.0f);
  Task taskb([] (double a, std::vector<double> v) {
    for (size_t i =  0; i < 10; ++i)
      v.push_back(a);
  }, 3.14f, std::vector<double>());

  EXPECT_TRUE(taska.executor != nullptr);
  EXPECT_TRUE(taskb.executor != nullptr);
}

TEST(TaskTests, CanCreateModifyReferencesByCaptureInExecution) {
  using Task = Voxx::Conky::Task<64>;
  std::vector<double> vec;

  Task task([&vec] (double a) {
    for (size_t i =  0; i < 10; ++i)
      vec.push_back(a);
  }, 3.14);

  EXPECT_TRUE(task.executor != nullptr);

  task.executor->execute();
  for (const auto& element : vec)
    EXPECT_EQ(element, 3.14);
}

TEST(TaskTests, CanCopyTasks) {
  using Task = Voxx::Conky::Task<128>;
  int result = 0;

  Task task([&result] {
    result++;
  });
  task.executor->execute();
  EXPECT_EQ(result, 1);

  auto copiedTask = task;
  copiedTask.executor->execute();
  EXPECT_EQ(result, 2);
}

// Although this specific implementation is not how tasks should be used (where
// multiple tasks need mutual exclusion for some of the task data), it does
// correctly test that the same task can be executed from multiple threads
// without error, and that tasks can be copied between threads.
TEST(TaskTests, CanInvokeTasksFromMultipleThreads) {
  Voxx::Thread::setAffinity(0);
  std::vector<Voxx::Conky::Task<64>> tasks;
  barrier.store(0);
  int result = 0, iters = 10;

  tasks.push_back([&result] {
    std::lock_guard<std::mutex> guard(exclusive);
    result++;
  });

  auto t1 = std::thread([&tasks, iters] {
    Voxx::Thread::setAffinity(1);
    barrier.fetch_add(1, std::memory_order_relaxed);
    while (barrier.load(std::memory_order_relaxed) != 2) {}

    for (int i = 0; i < iters; ++i)
      tasks.front().executor->execute();

    // Check copying to thread:
    auto task = tasks.front();
    for (int i = 0; i < iters; ++i)
      task.executor->execute();
  });
  auto t2 = std::thread([&tasks, iters] {
    Voxx::Thread::setAffinity(2);
    barrier.fetch_add(1, std::memory_order_relaxed);
    while (barrier.load(std::memory_order_relaxed) != 2) {}

    for (int i = 0; i < iters; ++i)
      tasks.front().executor->execute();

    // Check copying to thread:
    auto task = tasks.front();
    for (int i = 0; i < iters; ++i)
      task.executor->execute();
  }); 

  t1.join();
  t2.join();

  EXPECT_EQ(result, iters * 4);
}

int main(int argc, char** argv) {
  Voxx::System::CpuInfo::refresh();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}