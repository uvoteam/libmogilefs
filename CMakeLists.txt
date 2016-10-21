cmake_minimum_required(VERSION 2.8)

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11")
find_library(BOOST_SYSTEM boost_system REQUIRED)
find_library(CURL curl REQUIRED)

add_library(mogilefs
  mogilefs.cc
  )

target_link_libraries(mogilefs ${BOOST_SYSTEM} ${CURL})
