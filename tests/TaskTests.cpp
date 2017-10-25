//==--- tests/TaskTests.cpp -------------------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//  
//==------------------------------------------------------------------------==//
//
/// \file  TaskTests.cpp
/// \brief This file defines tests for the task class.
// 
//==------------------------------------------------------------------------==//

#include <Voxel/Conky/Task/Task.hpp>
#include <Voxel/SystemInfo/SystemInfo.hpp>
#include <gtest/gtest.h>

using Task = Voxx::Conky::Task<Voxx::System::destructiveInterferenceSize()*2>;

TEST(Executor, CanMakeAndExecuteStorableExecutor) {
  int result = 0;
  auto executor = Voxx::Conky::makeStorableExecutor(
    [&result] (int a, int b) {
      result = a + b;
    }, 
    1, 2
  );

  EXPECT_EQ(result, 0);
  executor.execute();
  EXPECT_EQ(result, 3);
}

TEST(Executor, CanMakeAndExecuteStorableExecutorVariadicArgs) {
  std::size_t result = 0;
  auto executor = Voxx::Conky::makeStorableExecutor(
    [&result] (auto&&... args) {
      result = sizeof...(args);
    }, 
    1, 2, 3, 4
  );

  EXPECT_EQ(result, 0);
  executor.execute();
  EXPECT_EQ(result, 4);
}

TEST(Executor, CanStoreDifferentSignatureExecutorsInContainer) {
  using ExecutorPtr       = std::unique_ptr<Voxx::Conky::Executor>;
  using ExecutorContainer = std::vector<ExecutorPtr>;

  enum class MultOrAdd {
    Mult, Add
  };

  int       result = 0;
  MultOrAdd op     = MultOrAdd::Mult;

  ExecutorContainer executors(0);
  executors.emplace_back(
    std::move(
      Voxx::Conky::makeUniqueStorableExecutor(
        [&result] (int a, int b) { 
          result = a + b;
        }
        , 5, 10
      )
    )
  );
  executors.emplace_back(
    std::move(
      Voxx::Conky::makeUniqueStorableExecutor(
        [&result, &op] (int a, int b, int c) {
          result = (op == MultOrAdd::Mult) ? a * b * c : a + b + c;
        }
        , 10, 20, 30
      )
    )
  );
  EXPECT_EQ(result, 0);

  executors.front()->execute();
  EXPECT_EQ(result, 5 + 10);

  executors.back()->execute();
  EXPECT_EQ(result, 10 * 20 * 30);
  op = MultOrAdd::Add;
  executors.back()->execute();
  EXPECT_EQ(result, 10 + 20 + 30);
}

struct Tester {
  std::vector<double> v;
  int x;
  float y;
  int* z;
};


TEST(Task, CanDefaultConstructTasksAndSetExecutor) {
  auto task  = Task();
  int result = 0;
  task.setExecutor([&result] (int a, int b, int c) {
    result = a + b + c;
  }, 1, 2, 3);

  EXPECT_EQ(result, 0);
  task.execute();
  EXPECT_EQ(result, 6);
}

TEST(Task, CanCreatTaskWithLambda) {
  int result = 0;
  auto task  = Task([&result] (int a, int b, int c) {
    result = a + b + c;
  }, 1, 2, 3);
  EXPECT_EQ(result, 0);
  task.execute();
  EXPECT_EQ(result, 6); 
}

TEST(Task, CanCreateAndRunContinuation) {
  int result = 0;
  auto task  = Task([&result] (int a, int b, int c) {
    result = a + b + c;
  }, 1, 2, 3);
  task.then([&result] (int a, float b) {
    result += a * static_cast<int>(b);
  }, 3, 4);
  EXPECT_EQ(result, 0);
  task.execute();
  EXPECT_EQ(result, 18); 
}

int main(int argc, char** argv) {
  Voxx::System::CpuInfo::refresh();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
