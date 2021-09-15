cmake_minimum_required(VERSION 3.10)
project(Block5 C)

set(CMAKE_C_STANDARD 11)

set(GCC_ANONYMOUS_ELEMENTS_FLAG	"-fms-extensions")
set(GCC_DEBUG_INFORMATION "-g")

add_definitions(${GCC_ANONYMOUS_ELEMENTS_FLAG} ${GCC_DEBUG_INFORMATION})

add_executable(peer peer.c ./CFileIncludes/protocol.c ./CFileIncludes/table.c ./CFileIncludes/net.c ./CFileIncludes/peer_struct.c ./CFileIncludes/ctrl_handlers.c ./CFileIncludes/fingertable_op.c)
target_link_libraries(peer pthread)
