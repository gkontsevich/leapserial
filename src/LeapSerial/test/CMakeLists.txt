set(LeapSerialTest_SRCS
  AESStreamTest.cpp
  ArchiveJSONTest.cpp
  BoundedStreamTest.cpp
  BufferedStreamTest.cpp
  ChronoTypesTest.cpp
  CompressionStreamTest.cpp
  InheritanceTest.cpp
  ArchiveLeapSerialTest.cpp
  MapTest.cpp
  MemoryStreamTest.cpp
  OptionalTest.cpp
  PathologicalTest.cpp
  PrettyPrintTest.cpp
  SerialCallbackTest.cpp
  SerialFormatTest.cpp
  SerializationTest.cpp
  SerializerEnumerationTest.cpp
  TestObject.h
  TestProtobufLS.hpp
)
add_pch(LeapSerialTest_SRCS "stdafx.h" "stdafx.cpp")

find_package(FlatBuffers QUIET)
if(FlatBuffers)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/TestObject_generated.h
    COMMAND FlatBuffers::flatc -c -I ${CMAKE_CURRENT_SOURCE_DIR} -o ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/TestObject.fbs
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/TestObject.fbs
    COMMENT "Generating TestObject_generated.h"
    VERBATIM
  )

  add_conditional_sources(LeapSerialTest_SRCS "TRUE"
    GROUP_NAME "Flatbuffers"
    FILES ${CMAKE_CURRENT_BINARY_DIR}/TestObject_generated.h
  )
  set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/TestObject_generated.h
    PROPERTY GROUP_NAME "Flatbuffers"
  )
endif()

add_conditional_sources(
  LeapSerialTest_SRCS
  "FlatBuffers"
  GROUP_NAME "FlatBuffers"
  FILES ArchiveFlatbufferTest.cpp TestObject.fbs
)

option(LS_ENABLE_PROTOBUF_TESTS "Enable protobuf tests" ON)
option(LS_PROTOBUF_REQUIRED "Require protobuf tests" OFF)
if(LS_ENABLE_PROTOBUF_TESTS OR LS_PROTOBUF_REQUIRED)
  if(LS_PROTOBUF_REQUIRED)
    find_package(Protobuf 3.0.0.2 EXACT REQUIRED)
  else()
    find_package(Protobuf 3.0.0.2 QUIET)
  endif()
endif()

if(Protobuf_FOUND)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/TestProtobuf.pb.cc ${CMAKE_CURRENT_BINARY_DIR}/TestProtobuf.pb.h
    COMMAND protobuf::protoc
    ARGS -I ${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/TestProtobuf.proto
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/TestProtobuf.proto
    COMMENT "Generating TestProtobuf.pb.cc and TestProtobuf.pb.h"
    VERBATIM
  )

  add_conditional_sources(LeapSerialTest_SRCS "TRUE"
    GROUP_NAME "Protobuf"
    FILES ${CMAKE_CURRENT_BINARY_DIR}/TestProtobuf.pb.h ${CMAKE_CURRENT_BINARY_DIR}/TestProtobuf.pb.cc
  )
endif()

add_conditional_sources(
  LeapSerialTest_SRCS
  "Protobuf_FOUND"
  GROUP_NAME "Protobuf"
  FILES ArchiveProtobufTest.cpp SchemaTest.cpp TestProtobuf.proto
)

find_package(Threads)
add_executable(LeapSerialTest ${LeapSerialTest_SRCS} ../../gtest-all-guard.cpp)
target_link_libraries(LeapSerialTest LeapSerial ${CMAKE_THREAD_LIBS_INIT})
add_test(NAME LeapSerialTest COMMAND $<TARGET_FILE:LeapSerialTest>)

if(FlatBuffers_FOUND)
  target_link_libraries(LeapSerialTest FlatBuffers::FlatBuffers)
endif()

if(Protobuf_FOUND)
  target_link_libraries(LeapSerialTest protobuf::protobuf)
  target_include_directories(LeapSerialTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()
