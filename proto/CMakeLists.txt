include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER txo.proto)

#add to programs
add_library(proto ${PROTO_HEADER} ${PROTO_SRC})
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})