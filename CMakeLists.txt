cmake_minimum_required(VERSION 3.16)
project(cache_packet_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

# Core cache model, shared by both simulators and the tests.
add_library(cache_core STATIC
    cache_sim/Cache.cpp
    cache_sim/CacheHierarchy.cpp
)
target_include_directories(cache_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Packet-side model on top of the cache core.
add_library(packet_core STATIC
    packet_sim/RingBuffer.cpp
    packet_sim/PacketProcessor.cpp
)
target_link_libraries(packet_core PUBLIC cache_core)

add_executable(cache_sim cache_sim/main.cpp)
target_link_libraries(cache_sim PRIVATE cache_core)

add_executable(tracegen cache_sim/TraceGenerator.cpp)

add_executable(packet_sim packet_sim/main.cpp)
target_link_libraries(packet_sim PRIVATE packet_core)

add_executable(packetgen packet_sim/PacketGenerator.cpp)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
