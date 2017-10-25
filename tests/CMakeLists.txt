#==--- tests/CMakeLists.txt -------------------------------------------------==#

find_package(GTest REQUIRED)

voxx_add_definitions(${Voxel_DEFINITIONS})
voxx_include_directories(${GTEST_INCLUDE_DIRS}
                         ${Voxel_INCLUDE_DIRS}
                         ${Conky_SOURCE_DIR}/include
                        /opt/intel/tbb/include)
voxx_library_directories(${Voxel_LIBRARY_DIRS})

#==--- Unit Tests -----------------------------------------------------------==#

voxx_add_executable(ContainerTests ${Conky_SOURCE_DIR}/tests/ContainerTests.cpp)
voxx_target_link_libraries(ContainerTests 
                           ${GTEST_BOTH_LIBRARIES}
                           ${Voxel_LIBRARIES})

voxx_add_executable(TaskTests
                    ${Conky_SOURCE_DIR}/tests/TaskTests.cpp)
voxx_target_link_libraries(TaskTests
                           ${GTEST_BOTH_LIBRARIES}
                           ${Voxel_LIBRARIES})

voxx_add_executable(ThreadPoolTests
                    ${Conky_SOURCE_DIR}/tests/ThreadPoolTests.cpp)
voxx_target_link_libraries(ThreadPoolTests
                           ${GTEST_BOTH_LIBRARIES}
                           ${Voxel_LIBRARIES})

#==--- Benchmarks -----------------------------------------------------------==#

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /opt/google)
find_package(benchmark)
if (benchmark_FOUND)
  set(benchmark_ROOT         ${benchmark_DIR}/../../..)
  set(benchmark_INCLUDE_DIRS ${benchmark_ROOT}/include)
  set(benchmark_LIBRARY_DIRS ${benchmark_ROOT}/lib    )
  set(benchmark_LIBRARIES    -lbenchmark              )

  voxx_library_directories_append(${benchmark_LIBRARY_DIRS})
  voxx_library_directories_append(/opt/intel/tbb/lib/intel64/gcc4.7)
  voxx_include_directories_append(${benchmark_INCLUDE_DIRS})

  voxx_add_executable(ContainerBenchmarks
                      ${Conky_SOURCE_DIR}/tests/ContainerBenchmarks.cpp)
  voxx_target_link_libraries(ContainerBenchmarks
                             ${Voxel_LIBRARIES}
                             ${benchmark_LIBRARIES})

  voxx_add_executable(TaskBenchmarks
                      ${Conky_SOURCE_DIR}/tests/TaskBenchmarks.cpp)
  voxx_target_link_libraries(TaskBenchmarks 
                             ${Voxel_LIBRARIES}
                             ${benchmark_LIBRARIES})

  voxx_add_executable(ThreadPoolBenchmarks
                      ${Conky_SOURCE_DIR}/tests/ThreadPoolBenchmarks.cpp)
  voxx_target_link_libraries(ThreadPoolBenchmarks 
                             ${Voxel_LIBRARIES}
                             ${benchmark_LIBRARIES} -lc++abi)
endif(benchmark_FOUND)