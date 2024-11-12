cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_FLAGS "-std=c++17")
project(envy)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(envy main.cpp Node.cpp duktape.c Script.cpp User.cpp)
include_directories(${SQLite3_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
target_link_libraries (envy ${SQLite3_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARY})