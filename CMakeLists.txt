cmake_minimum_required(VERSION 3.29)
project(video_codec)

set(CMAKE_CXX_STANDARD 26)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AV REQUIRED libavcodec libavformat libavutil libswscale libavfilter)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${AV_INCLUDE_DIRS}
)

link_directories(${AV_LIBRARY_DIRS})

set(SOURCES
    src/main.cpp
    src/media/media_file.cpp
    src/media/video_stream.cpp
    src/processing/simple_frame_processor.cpp
)

add_executable(video_codec ${SOURCES})

target_link_libraries(video_codec 
    ${AV_LIBRARIES}
)
