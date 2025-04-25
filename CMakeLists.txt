cmake_minimum_required(VERSION 3.29)
project(video_codec)

set(CMAKE_CXX_STANDARD 26)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AV REQUIRED libavcodec libavformat libavutil libswscale libavfilter)

find_package(OpenCV REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${AV_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

link_directories(${AV_LIBRARY_DIRS})

set(SOURCES
    src/main.cpp
    src/media/media_file.cpp
    src/media/video_stream.cpp
)

add_executable(video_codec ${SOURCES})

target_link_libraries(video_codec 
    ${AV_LIBRARIES}
    ${OpenCV_LIBS}
)

# DEBUG
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")