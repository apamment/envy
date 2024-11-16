cmake_minimum_required(VERSION 3.6)
project(jamlib)

add_definitions(-D__UNIX__ -DHAVE_SYS_FILE_H -DHAVE_UNISTD_H -DHAVE_FCNTL)
set(SOURCE_FILES src/crc32.c src/lastread.c src/mbase.c src/message.c src/structrw.c src/subpack.c)
include_directories(./include)
add_library(jamlib STATIC ${SOURCE_FILES})