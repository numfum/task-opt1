cmake_minimum_required(VERSION 3.0)

project(task-opt1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g3 -D_DEBUG=1 -Wno-unused")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g0 -DNDEBUG=1 -flto")

if (EMSCRIPTEN)
	set(CMAKE_EXE_LINKER_FLAGS "-s SINGLE_FILE=1")
endif()

add_executable(task-opt1 "${CMAKE_CURRENT_LIST_DIR}/main.cpp")

target_include_directories(task-opt1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
