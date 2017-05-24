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

#include <Voxel/Conky/Concurrent/CircularDequeue.hpp>
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

/// Fixture class for work stealing queue tests.
class CircularDequeueTest : public ::testing::Test {
 protected:
  /// Defines the number of elements in the dequeue.
  static constexpr std::size_t queueSize    = 1 << 25;
  /// Defines the number of element to push onto the queue.
  static constexpr std::size_t testElements = queueSize << 2;

  /// Alias for the type of data in the queue.
  using DataType   = int;
  /// Alias for the type of the queue.
  using QueueType  = Voxx::Conky::CircularDequeue<DataType, queueSize>;
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
      while (queue.size() == queueSize) { /* Full queue, spin. */ }
      queue.push(i);

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
    threads.emplace_back(&CircularDequeueTest::pushAndPop, this, 0, cores);
    for (std::size_t i = 1; i < cores; ++i)
      threads.emplace_back(&CircularDequeueTest::steal, this, i, cores);
  }

  /// Joins the threads.
  void join() {
    for (auto& thread : threads)
      thread.join();
  }

  /// Set's up the results containers.
  void setUp() {
    for (std::size_t i = 0; i < Voxx::System::CpuInfo::cores(); ++i)
      results.push_back(ResultType());
  }

  // Prints the first 200 results for each thread.
  void printResults() {
    int resultNumber = 0;
    for (const auto& result : results) {
      std::cout << "Results : " << resultNumber << "\n";
      std::size_t stopper = 0;
      for (const auto& r : result) {
        if (stopper++ > 200)
          break;
        std::cout << r << "\n";
      }
      std::cout << "\n";
      resultNumber++;
    }
  }

  QueueType                queue;       //!< The queue to test.
  std::vector<ResultType>  results;     //!< Vectors of results for each thread.
  std::vector<std::thread> threads;     //!< Thread to test the queue with.
  std::mutex               resultMutex; //!< Mutex for pushing results.
};

TEST_F(CircularDequeueTest, CorrectlyDeterminesSize) {
  generate(queueSize >> 1);
  EXPECT_EQ(queue.size(), (queueSize >> 1));
}

TEST_F(CircularDequeueTest, CanPopSingleThreaded) {
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

// This test is designed to test if the CircularDequeue breaks. There are two
// cases which "break" the deque:
//
// 1. Pushing onto the deque when it is full: This is defined in the dequeue's
//    API, as it would require an extra check on each push to the dequeue.
//    The API requires that the user simply specify a large enough dequeue,
//    which should neven be a problem on current hardware. DynamicDequeue may be
//    added which can be resized.
//
// 2. Concurrent popping and stealing: If there is a single element in the deque
//    then the thread owning the deque (the one which may push), will be in a
//    race with the other threads (which are trying to steal) for the last 
//    element. An incorrect implementation would allow pops and steals to
//    access the same element.
//
// To test case 2, this test has the first thread pusing to the dequeue, and
// occasionally popping, while other threads steal. As there will be more
// threads stealing, as well as the one thread having to push and pop, the
// dequeue will not grow and there will be constant contention to get the first
// element.
//
// Each thread stores the results of popped or stolen items, and ifno item
// appears in multiple thread's results, then there has not been any error.
TEST_F(CircularDequeueTest, PopAndStealRaceCorrectly) {
  Voxx::System::CpuInfo::refresh();
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}