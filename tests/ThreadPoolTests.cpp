//==--- tests/ThreadPoolTests.hpp -------------------------- -*- C++ -*- ---==//
//            
//                                    Voxel
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//  
//==------------------------------------------------------------------------==//
//
/// \file  ThreadPoolTests.cpp
/// \brief This file defines tests for the thread pool classes.
// 
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Container/ThreadPool.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <Voxel/Thread/Thread.hpp>
#include <gtest/gtest.h>

static constexpr std::size_t poolQueueSize = 2048;
using ThreadPool = Voxx::Conky::ThreadPool<poolQueueSize>;

TEST(ThreadPool, CanPushGenericWorkOntoThreadPool) {
  ThreadPool threadPool(Voxx::System::CpuInfo::cores());

  int intCount = 0, iterations = 100;
  for (int i = 0; i < iterations; ++i) {
    threadPool.tryPush([&intCount] {
      intCount += Voxx::Conky::Thread::threadId + 1;
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  threadPool.stopThreads();

  // Since the values are updated non-atomically, they could be anything,
  // this test is really just to check that different callable types can
  // update data once push onto the queue.
  EXPECT_GT(intCount, iterations);
}

TEST(ThreadPool, CanHandleheavySubmission) {
  ThreadPool threadPool(Voxx::System::CpuInfo::cores());

  int result = 0;
  for (std::size_t i = 0; i < (poolQueueSize << 6); ++i) {
    threadPool.tryPush([&result, i] {
      result = i;
    });
  }

  while (!threadPool.isEmpty()) { /* Spin ... */ }
  threadPool.stopThreads();

  EXPECT_GT(result, poolQueueSize - Voxx::System::CpuInfo::cores()); 
}

TEST(ThreadPool, CanStartAndStopThreads) {
  ThreadPool threadPool(Voxx::System::CpuInfo::cores());

  EXPECT_EQ(threadPool.runningThreads(), Voxx::System::CpuInfo::cores());
  threadPool.stopThreads();
  EXPECT_EQ(threadPool.runningThreads(), 0);
  threadPool.startThreads();
  EXPECT_EQ(threadPool.runningThreads(), Voxx::System::CpuInfo::cores());
}

int main(int argc, char** argv) {
  Voxx::System::CpuInfo::refresh();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
