cmake_minimum_required(VERSION 3.10)
project(EntityPivotingEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pivot src/main.cpp src/database.cpp src/dns_fetcher.cpp src/exporter.cpp)

find_package(SQLite3 REQUIRED)
#find_package(c-ares REQUIRED)
target_include_directories(pivot PRIVATE third_party)
target_link_libraries(pivot PRIVATE SQLite::SQLite3) # c-ares::c-ares)

enable_testing()
add_executable(test_init tests/test_init.cpp src/database.cpp)
target_link_libraries(test_init PRIVATE SQLite::SQLite3)
add_test(NAME test_init COMMAND test_init)
