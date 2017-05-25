#==--- tests/CMakeLists.txt -------------------------------------------------==#

find_package(GTest REQUIRED)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /opt/google)
find_package(benchmark)
if (benchmark_FOUND)
  set(benchmark_ROOT         ${benchmark_DIR}/../../..)
  set(benchmark_INCLUDE_DIRS ${benchmark_ROOT}/include)
  set(benchmark_LIBRARY_DIRS ${benchmark_ROOT}/lib    )
  set(benchmark_LIBRARIES    -lbenchmark              )
endif()

voxx_add_definitions(${Voxel_DEFINITIONS})
voxx_include_directories(${GTEST_INCLUDE_DIRS}
                         ${Voxel_INCLUDE_DIRS}
                         ${benchmark_INCLUDE_DIRS}
                         ${Conky_SOURCE_DIR}/include)
voxx_library_directories(${Voxel_LIBRARY_DIRS})

#==--- Unit Tests -----------------------------------------------------------==#

voxx_add_executable(ContainerTests ${Conky_SOURCE_DIR}/tests/ContainerTests.cpp)
voxx_target_link_libraries(ContainerTests 
                           ${GTEST_BOTH_LIBRARIES}
                           ${Voxel_LIBRARIES})

voxx_library_directories_append(${benchmark_LIBRARY_DIRS})
voxx_add_executable(ContainerBenchmarks
                    ${Conky_SOURCE_DIR}/tests/ContainerBenchmarks.cpp)
voxx_target_link_libraries(ContainerBenchmarks 
                           ${benchmark_LIBRARIES}
                           ${Voxel_LIBRARIES})