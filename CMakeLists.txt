cmake_minimum_required(VERSION 3.28)
project(uDataPacketService VERSION 0.0.1 LANGUAGES CXX)
enable_testing()

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_FOR_DEBIAN "Build debian container for deployment" OFF)
option(BUILD_TESTS "Compile unit tests" ON) 
include(GenerateExportHeader)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
if (NOT ${BUILD_SHARED_LIBS})
   set(Boost_USE_STATIC_LIBS ON) 
endif()
find_package(absl CONFIG REQUIRED)
find_package(utf8_range CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(opentelemetry-cpp REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED headers program_options REQUIRED)
find_package(TBB COMPONENTS tbb REQUIRED)
find_package(Threads REQUIRED)
if (BUILD_TESTS)
   find_package(Catch2 3 REQUIRED)
endif()

set(IMPORT_PROTO_SRC
    uDataPacketImportAPI/v1/data_type.proto
    uDataPacketImportAPI/v1/stream_identifier.proto
    uDataPacketImportAPI/v1/packet.proto
    uDataPacketImportAPI/v1/subscription_request.proto
    uDataPacketImportAPI/v1/backend.proto)
set(EXPORT_PROTO_SRC
    uDataPacketServiceAPI/v1/data_type.proto
    uDataPacketServiceAPI/v1/stream_identifier.proto
    uDataPacketServiceAPI/v1/packet.proto
    )
set(LIBRARY_SRC
    src/subscriberOptions.cpp
    src/subscriber.cpp
    ${IMPORT_PROTO_SRC}
    ${EXPORT_PROTO_SRC})
set(HEADER_FILES
    include/uDataPacketService/subscriber.hpp
    include/uDataPacketService/subscriberOptions.hpp)

add_library(libuDataPacketService STATIC ${LIBRARY_SRC})
add_library(uDataPacketService::libuDataPacketService ALIAS libuDataPacketService)
set_target_properties(libuDataPacketService PROPERTIES
                      OUTPUT_NAME uDataPacketService
                      CXX_STANDARD 20
                      CXX_STANDARD_REQUIRED YES 
                      CXX_EXTENSIONS NO) 
target_include_directories(libuDataPacketService
                           PRIVATE $<INSTALL_INTERFACE:include>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                          )
target_link_libraries(libuDataPacketService
                      PUBLIC TBB::tbb
                      PRIVATE gRPC::grpc
                              gRPC::grpc++
                              protobuf::libprotobuf
                              opentelemetry-cpp::otlp_http_log_record_exporter
                              opentelemetry-cpp::otlp_http_metric_exporter
                              spdlog::spdlog_header_only
                              Boost::headers
                              #Boost::program_options
                              Threads::Threads)
target_sources(libuDataPacketService
               PUBLIC FILE_SET
                      HEADERS
                      BASE_DIRS
                         include
                      FILES
                         ${HEADER_FILES}
               PUBLIC FILE_SET
                      CXX_MODULES
                      BASE_DIRS
                         src/modules
                      FILES
                         ${MODULE_FILES}
               )
protobuf_generate(TARGET libuDataPacketService
                  LANGUAGE cpp
                 )
protobuf_generate(TARGET libuDataPacketService
                  LANGUAGE grpc
                  PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
                  PLUGIN_OPTIONS generate_mock_code=true
                  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
                 )

add_executable(uDataPacketService
               src/main.cpp)
set_target_properties(uDataPacketService PROPERTIES
                      CXX_STANDARD 20
                      CXX_STANDARD_REQUIRED YES 
                      CXX_EXTENSIONS NO) 
target_include_directories(uDataPacketService
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
                          )
target_link_libraries(uDataPacketService
                      PUBLIC
                         TBB::tbb
                      PRIVATE
                         uDataPacketService::libuDataPacketService
                         gRPC::grpc
                         gRPC::grpc++
                         opentelemetry-cpp::otlp_http_log_record_exporter
                         opentelemetry-cpp::otlp_http_metric_exporter
                         spdlog::spdlog_header_only
                         Boost::headers
                         Boost::program_options
                         Threads::Threads)
