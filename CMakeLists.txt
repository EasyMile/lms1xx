cmake_minimum_required (VERSION 2.8.9)

#--------------------------------------------------------------------------------------------------#

project (liblms1xx)

#--------------------------------------------------------------------------------------------------#

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(NOT COMPILER_SUPPORTS_CXX11)
  message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support.")
endif ()

#--------------------------------------------------------------------------------------------------#

OPTION( BUILD_test "Build test application" OFF )

#--------------------------------------------------------------------------------------------------#

find_package(Threads)
find_package(Boost 1.54.0 REQUIRED COMPONENTS system thread)

if (NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found")
endif ()

message("-- Boost Include: ${Boost_INCLUDE_DIR}")
message("-- Boost Libraries: ${Boost_LIBRARY_DIRS}")

#--------------------------------------------------------------------------------------------------#

set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++11 ${CMAKE_CXX_FLAGS}")

#--------------------------------------------------------------------------------------------------#

include_directories(SYSTEM "${Boost_INCLUDE_DIRS}")
include_directories("${PROJECT_SOURCE_DIR}")

#--------------------------------------------------------------------------------------------------#

add_library(lms1xx STATIC ${PROJECT_SOURCE_DIR}/lms1xx/lms1xx.cc)
target_link_libraries(lms1xx ${Boost_LIBRARIES})

#--------------------------------------------------------------------------------------------------#

install(TARGETS lms1xx DESTINATION lib)
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/lms1xx DESTINATION include
  FILES_MATCHING PATTERN "*.hh"
)

#--------------------------------------------------------------------------------------------------#

if (BUILD_test)
  add_executable(test_run "${PROJECT_SOURCE_DIR}/test/test_run.cc")
  target_link_libraries(test_run lms1xx ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif ()

#--------------------------------------------------------------------------------------------------#
