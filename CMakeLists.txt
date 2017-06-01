#==--- Conky/CMakeLists.txt -------------------------------------------------==#

cmake_minimum_required(VERSION 3.4)
project(Conky)

#==--- Versioning -----------------------------------------------------------==#

set(Conky_VERSION 0.1.0)

#==--- Build Type -----------------------------------------------------------==#

if(NOT CMAKE_BUILD_TYPE) 
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type not specendified!" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Check if the install prefix has Voxel appended, and if not, then add it:
string(REGEX MATCH ".*Voxel?" PREFIX_MATCH "${CMAKE_INSTALL_PREFIX}")
if ("${PREFIX_MATCH}" STREQUAL "")
  set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/Voxel" CACHE STRING
      "Install prefix" FORCE)
endif()

#==--- Options --------------------------------------------------------------==#

set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${Conky_SOURCE_DIR}/cmake)

#==--- Packages -------------------------------------------------------------==#

find_package(Voxel REQUIRED)

#==--- Compiler Flags -------------------------------------------------------==#

if(WIN32)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
  set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} /O2"  )
else()
  set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}" )
endif()

#==--- Set CXXFLAGS ---------------------------------------------------------==#

if(CMAKE_BUILD_TYPE MATCHES Release)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -g"
      CACHE STRING "CXX FLAGS" FORCE)
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "CXX FLAGS" FORCE)
endif()

#==--- Header install -------------------------------------------------------==#

install(DIRECTORY   ${Conky_SOURCE_DIR}/include/Voxel/Conky
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Voxel)

#==--- Subdirectories -------------------------------------------------------==#

add_subdirectory(docs)
add_subdirectory(tests)
#add_subdirectory(src)

voxx_create_all_targets()

#==--- Install --------------------------------------------------------------==#

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/Conky/ConkyConfigVersion.cmake"
  VERSION       ${Conky_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_file(cmake/ConkyConfig.cmake
               "${CMAKE_CURRENT_BINARY_DIR}/Conky/ConkyConfig.cmake"
               COPYONLY)

# This is relative to CMAKE_INSTALL_PREFIX:
set(ConfigPackageLocation lib/cmake/Voxel)
install(
  FILES       cmake/ConkyConfig.cmake
              "${CMAKE_CURRENT_BINARY_DIR}/Conky/ConkyConfigVersion.cmake"
  DESTINATION ${ConfigPackageLocation})

#==--- Print Configuration --------------------------------------------------==#

message(
"\n|==---------------------------------------------------------------------==|")
message(
  "| Configuration:                                                          |")
message(
  "|==---------------------------------------------------------------------==|")
message("| Conky VERSION           : ${Conky_VERSION}"                         )
message("| BUILD TYPE              : ${CMAKE_BUILD_TYPE}"                      )
message("| CMAKE_CXX_FLAGS_RELEASE : ${CMAKE_CXX_FLAGS_RELEASE}"               )
message("| CMAKE_CXX_FLAGS_DEBUG   : ${CMAKE_CXX_FLAGS_DEBUG}"                 )
message("| CMAKE_CXX_FLAGS         : ${CMAKE_CXX_FLAGS}"                       )
message("| INSTALL_PREFIX          : ${CMAKE_INSTALL_PREFIX}"                  )
message("| VOXEL FOUND             : ${Voxel_FOUND}"                           )
message("| VOXEL ROOT              : ${Voxel_DIR}"                             )
message("| CUDA SUPPORTED          : ${CUDA_FOUND}"                            )
message("| CUDA ROOT               : ${CUDA_TOOLKIT_ROOT_DIR}"                 )
message("| PHYSICAL CORES          : ${VOXX_PHYSICAL_CORES}"                   )
message("| LOGICAL CORES           : ${VOXX_LOGICAL_CORES}"                    )
message("| CACHELINE SIZE          : ${VOXX_CACHELINE_SIZE}"                   )
message("| CPU INTRINSICS          : ${VOXX_INTRINSICS}"                       )
message(
"|==---------------------------------------------------------------------==|\n")
