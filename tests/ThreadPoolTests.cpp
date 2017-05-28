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

#include <Voxel/Conky/Container/BalancedThreadPool.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <Voxel/Thread/Thread.hpp>
#include <gtest/gtest.h>

using ThreadPool = Voxx::Conky::BalancedThreadPool<256>;

TEST(BalancedThreadPool, CanPushGenericWorkOntoThreadPool) {
  Voxx::System::CpuInfo::refresh();
  ThreadPool threadPool(Voxx::System::CpuInfo::cores());

  int   intCount   = 0, iterations = 100;
  float floatCount = 0.0f;
  for (int i = 0; i < iterations; ++i) {
    if (i & 1) {
      threadPool.push([&intCount] {
        intCount += 2;
      });
      continue;
    }
    threadPool.push([&floatCount] {
      floatCount += 1.0f;
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  threadPool.shutdown(); 

  // Since the values are updated non-atomically, they could be anything,
  // this test is really just to check that different callable types can
  // update data once push onto the queue.
  EXPECT_GT(intCount  , 0   );
  EXPECT_GT(floatCount, 0.0f);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
