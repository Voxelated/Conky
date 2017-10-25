//==--- Conky/Task/TaskManager.hpp ------------------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  TaskManager.hpp
/// \brief This file provides the definition the TaskManager class which is used
///        to manage tasks in the system.
//
//==------------------------------------------------------------------------==//

#ifndef VOXX_CONKY_TASK_TASK_MANAGER_HPP
#define VOXX_CONKY_TASK_TASK_MANAGER_HPP

#include <Voxel/Conky/Thread/ThreadPool.hpp>

namespace Voxx::Conky {

class TaskManager {
 public:
  //==--- Con/destruction --------------------------------------------------==//
  TaskManager() = default;

  TaskManager(const TaskManager&) = delete;
  TaskManager(TaskManager&&)      = delete;

  //==--- Operator Overloads -----------------------------------------------==//
  
  TaskManager& operator=(const TaskManager&) = delete;
  TaskManager& operator=(TaskManager&&)      = delete;

 private:
  DefaultThreadPool Workers;
};

} // namespace Voxx::Conky

#endif // VOXX_CONKY_TASK_TASK_MANAGER_HPP