cmake_minimum_required(VERSION 3.29)
project(video_codec)

set(CMAKE_CXX_STANDARD 26)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AV REQUIRED libavcodec libavformat libavutil libswscale libavfilter)

include_directories(${AV_INCLUDE_DIRS})
link_directories(${AV_LIBRARY_DIRS})

add_executable(video_codec main.cpp)
target_link_libraries(video_codec ${AV_LIBRARIES})