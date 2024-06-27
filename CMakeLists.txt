cmake_minimum_required(VERSION 3.10)
project(ChatServerClient)

set(CMAKE_CXX_STANDARD 14)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)


find_package(Boost REQUIRED COMPONENTS system thread)
include_directories(${Boost_INCLUDE_DIRS})

include(FetchContent)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.10.5
)

FetchContent_MakeAvailable(json)

include_directories(${CMAKE_SOURCE_DIR}/external)
include_directories(${CMAKE_SOURCE_DIR}/external/doctest)
include_directories(${CMAKE_SOURCE_DIR}/common)

configure_file(${CMAKE_SOURCE_DIR}/config/config.json ${CMAKE_BINARY_DIR}/config/config.json COPYONLY)

add_compile_definitions(SIGSTKSZ=8192)

add_executable(chat_server server/chat_server.cpp)
target_link_libraries(chat_server ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

add_executable(chat_client client/chat_client.cpp)
target_link_libraries(chat_client ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

enable_testing()

add_executable(test_client tests/test_client.cpp)
target_link_libraries(test_client ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)
add_test(NAME test_client COMMAND test_client)

add_executable(test_server tests/test_server.cpp)
target_link_libraries(test_server ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)
add_test(NAME test_server COMMAND test_server)
