//==--- tests/ThreadPoolTests.hpp -------------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
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

#include <Voxel/Conky/Thread/ThreadPool.hpp>
#include <gtest/gtest.h>

using namespace Voxx::Conky;

TEST(ThreadPool, CanPushGenericWorkOntoThreadPool) {
  DefaultThreadPool threadPool;
  threadPool.startThreads();

  int intCount = 0, iterations = 100;
  for (int i = 0; i < iterations; ++i) {
    threadPool.tryPush([&intCount, &threadPool] {
      intCount += threadPool.currentAffinity() + 1;
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  threadPool.stopThreads();

  // Since the values are updated non-atomically, they could be anything,
  // this test is really just to check that different callable types can
  // update data once push onto the queue.
  EXPECT_GT(intCount, iterations);
}

TEST(ThreadPool, CanHandleHeavySubmission) {
  DefaultThreadPool threadPool;
  threadPool.startThreads();

  int result = 0;
  for (std::size_t i = 0; i < (DefaultThreadPool::tasksPerQueue << 6); ++i) {
    threadPool.tryPush([&result, i] {
      result = i;
    });
  }

  while (!threadPool.isEmpty()) { /* Spin ... */ }
  threadPool.stopThreads();

  EXPECT_GT(result,
            DefaultThreadPool::tasksPerQueue - Voxx::System::CpuInfo::cores()); 
}

TEST(ThreadPool, CanStartAndStopThreads) {
  DefaultThreadPool threadPool;
  threadPool.startThreads();
  //Voxx::System::CpuInfo::cores());

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
