cmake_minimum_required(VERSION 3.16)
project(catsurf_webserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(catsurf
    main.cpp
    poller.cpp
)

target_include_directories(catsurf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
