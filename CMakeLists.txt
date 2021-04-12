cmake_minimum_required(VERSION 3.1)
project(epic_miner)

add_subdirectory(libuv)
add_subdirectory(randomx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -maes -msse4")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -maes -msse4")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenSSL 1.1 REQUIRED)

enable_language(ASM)

file(GLOB SRCFILES *.cpp net/*.cpp console/*.cpp)

add_executable(epic_miner ${SRCFILES} ${randomx})
target_include_directories(epic_miner SYSTEM BEFORE PUBLIC libuv/include)
target_include_directories(epic_miner PUBLIC randomx/src rapidjson/include .)
target_include_directories(epic_miner PUBLIC ${OPENSSL_INCLUDE_DIR})
target_compile_definitions(epic_miner PUBLIC NOMINMAX)

target_link_libraries(epic_miner Threads::Threads OpenSSL::SSL uv_a randomx)
if(MSVC)
    target_link_libraries(epic_miner crypt32.lib)
endif()
