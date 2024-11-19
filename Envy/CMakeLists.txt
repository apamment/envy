cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_FLAGS "-std=c++17 -g")
project(envy)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_subdirectory(jamlib-1.4.7)
add_executable(envy main.cpp Node.cpp duktape.c Script.cpp User.cpp Logger.cpp MessageBase.cpp FullScreenEditor.cpp Door.cpp Email.cpp)
include_directories(${SQLite3_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} jamlib-1.4.7/include)
target_link_libraries (envy ${SQLite3_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARY} jamlib)