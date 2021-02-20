cmake_minimum_required(VERSION 3.1)
project(epic_miner)

add_subdirectory(libuv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -maes -msse4")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -maes -msse4")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenSSL 1.1 REQUIRED)

enable_language(ASM)

file(GLOB randomx randomx/src/*.cpp randomx/src/*.c randomx/src/blake2/*.c randomx/src/*.S)
file(GLOB SRCFILES *.cpp net/*.cpp console/*.cpp)

add_executable(epic_miner ${SRCFILES} ${randomx})
target_include_directories(epic_miner PUBLIC randomx/src)
target_link_libraries(epic_miner Threads::Threads {OPENSSL_LIBRARIES})

target_include_directories(epic_miner SYSTEM BEFORE PUBLIC libuv/include)
target_include_directories(epic_miner PUBLIC randomx/src rapidjson/include .)

target_link_libraries(epic_miner Threads::Threads ${OPENSSL_LIBRARIES} uv_a)
