cmake_minimum_required (VERSION 3.15)
project (servo)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${servo_SOURCE_DIR}/../cmake/)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

add_executable(servo Servo.cpp IPBlockItem.cpp EventMgr.cpp)
if (CMAKE_SYSTEM_NAME MATCHES "Haiku")
  target_link_libraries (servo pthread network)
else()
 	target_link_libraries (servo util pthread)
endif()
