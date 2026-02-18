cmake_minimum_required(VERSION 3.30)
project(bump_allocator)

set(CMAKE_CXX_STANDARD 23)


#bump allocators
file(GLOB_RECURSE sources
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(bump_allocator ${sources})
target_include_directories(bump_allocator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(bump_allocator PUBLIC BUMP_TRACK_HEAP)


add_executable(main main.cpp ${bump_allocator})
target_link_libraries(main PRIVATE bump_allocator)

add_executable(example example1.cpp ${bump_allocator})
target_link_libraries(example PRIVATE bump_allocator)


include(cmake/clangformat.cmake)
file(GLOB_RECURSE headers ${CMAKE_CURRENT_SOURCE_DIR}/bump/*.h)
add_file_to_format(${sources} ${headers})
