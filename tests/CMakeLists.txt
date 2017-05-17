#==--- tests/CMakeLists.txt -------------------------------------------------==#

find_package(GTest REQUIRED)

voxx_add_definitions(${Voxel_DEFINITIONS})
voxx_include_directories(${GTEST_INCLUDE_DIRS}
                         ${Voxel_INCLUDE_DIRS}
                         ${Conky_SOURCE_DIR}/include)
voxx_library_directories(${Voxel_LIBRARY_DIRS})

#==--- Unit Tests -----------------------------------------------------------==#

voxx_add_executable(ContainerTests ${Conky_SOURCE_DIR}/tests/ContainerTests.cpp)
voxx_target_link_libraries(ContainerTests 
                           ${GTEST_BOTH_LIBRARIES}
                           ${Voxel_LIBRARIES})