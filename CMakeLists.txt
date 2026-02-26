cmake_minimum_required(VERSION 3.28)
project(uDataPacketService VERSION 0.0.1 LANGUAGES CXX)
enable_testing()

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_CONTAINER "Build container for deployment" OFF)
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
    uDataPacketServiceAPI/v1/subscription_request.proto
    uDataPacketServiceAPI/v1/subscribe_to_all_request.proto
    uDataPacketServiceAPI/v1/broadcast.proto
    )
set(LIBRARY_SRC
    src/grpcOptions.cpp
    src/subscriberOptions.cpp
    src/subscriber.cpp
    src/futurePacketDetector.cpp
    src/expiredPacketDetector.cpp
    src/duplicatePacketDetector.cpp
    src/stream.cpp
    src/streamOptions.cpp
    src/subscriptionManager.cpp
    src/subscriptionManagerOptions.cpp
    ${IMPORT_PROTO_SRC}
    ${EXPORT_PROTO_SRC}
    )
set(HEADER_FILES
    include/uDataPacketService/stream.hpp
    include/uDataPacketService/streamOptions.hpp
    include/uDataPacketService/subscriptionManager.hpp
    include/uDataPacketService/subscriptionManagerOptions.hpp
    include/uDataPacketService/subscriber.hpp
    include/uDataPacketService/subscriberOptions.hpp)
set(MODULE_FILES
    src/modules/programOptions.cppm
    src/modules/packetConverter.cppm
    src/modules/logger.cppm
    src/modules/otelSpdlogSink.cppm
    src/modules/utilities.cppm
    src/modules/asyncWriter.cppm
    )

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

##########################################################################################
#                                         Tests                                          #
##########################################################################################
if (PROJECT_IS_TOP_LEVEL AND ${BUILD_TESTS})
   add_executable(unitTests
                  testing/grpc.cpp
                  testing/packetConverter.cpp
                  testing/sanitizer.cpp
                  testing/stream.cpp)
   set_target_properties(unitTests PROPERTIES
                         CXX_STANDARD 20
                         CXX_STANDARD_REQUIRED YES 
                         CXX_EXTENSIONS NO) 
   target_link_libraries(unitTests
                         PUBLIC
                            TBB::tbb
                         PRIVATE
                            uDataPacketService::libuDataPacketService
                            gRPC::grpc
                            gRPC::grpc++
                            opentelemetry-cpp::otlp_http_metric_exporter
                            Threads::Threads
                            Catch2::Catch2 Catch2::Catch2WithMain)
   target_include_directories(unitTests
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
   add_test(NAME unitTests COMMAND $<TARGET_FILE:unitTests>)
endif()

##########################################################################################
#                                      Installation                                      #
##########################################################################################
include(CMakePackageConfigHelpers)
configure_file(${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}.cmake.in
               ${PROJECT_NAME}Config.cmake @ONLY)
write_basic_package_version_file(
   ${PROJECT_NAME}ConfigVersion.cmake
   VERSION ${PACKAGE_VERSION}
   COMPATIBILITY AnyNewerVersion)

include(GNUInstallDirs)
install(TARGETS libuDataPacketService
        EXPORT ${PROJECT_NAME}Targets
        FILE_SET HEADERS 
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILE_SET CXX_MODULES
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT libraries)
install(TARGETS uDataPacketService
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT applications)
export(EXPORT ${PROJECT_NAME}Targets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

##########################################################################################
#                                        Containers                                      #
##########################################################################################
include(CPackComponent)
if (${BUILD_CONTAINER})
   message("Will configure container")
   configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/Dockerfile.u24.in
                  ${CMAKE_CURRENT_BINARY_DIR}/Dockerfile.u24 @ONLY)
   add_custom_target(${PROJECT_NAME}-u24
                     COMMENT "Creating container file for ${PROJECT_NAME}"
                     COMMAND ${CMAKE_CPACK_COMMAND} -G DEB 
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} VERBATIM)
   add_dependencies(${PROJECT_NAME}-u24 uDataPacketService libuDataPacketService)
   find_program(buildah_EXECUTABLE buildah)
   if (buildah_EXECUTABLE)
      add_custom_target(buildah
                        COMMENT "Preparing the container with buildah"
                        COMMAND ${buildah_EXECUTABLE} bud -t udatapacketimportproxy:${PROJECT_VERSION} -t udatapacketimportproxy:latest -f ./Dockerfile.u24
                        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} VERBATIM)
      add_dependencies(buildah ${PROJECT_NAME}-u24)
   endif()
endif()

##########################################################################################
#                                     CPACK Packaging                                    #
##########################################################################################
include(CPackComponent)
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "UUSS")
set(CPACK_PACKAGE_CONTACT "ben.baker@utah.edu")
set(CPACK_PACKAGE_LICENSE "MIT")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Scrapes packets from the import proxy then serves packets to clients in the UUSS Kubernetes environment.")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
cpack_add_component(applications libraries
                    DISPLAY_NAME "uDataPacketService applications")
set(CPACK_GENERATOR ZIP TGZ)
if (WIN32)
   list(APPEND CPACK_GENERATOR WIX)
elseif (APPLE)
   list(APPEND CPACK_GENERATOR productbuild)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
   find_program(RPMBUILD_PROGRAM rpmbuild)
   if (RPMBUILD_PROGRAM)
      list(APPEND CPACK_GENERATOR RPM)
   endif()
   #if (${BUILD_CONTAINER})
   #   find_program(DPKG_PROGRAM dpkg REQUIRED)
   #else()
   #   find_program(DPKG_PROGRAM dpkg)
   #endif()
   #if (DPKG_PROGRAM)
   #   list(APPEND CPACK_GENERATOR DEB)
   #   set(CPACK_DEBIAN_PACKAGE_DEPENDS
   #       "${CPACK_DEBIAN_PACKAGE_DEPENDS}")
   #   set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
   #endif()
else()
   message("Unknown operating system")
endif()
set(CPACK_SOURCE_IGNORE_FILES
  /\\.git/
  \\.swp
  \\.orig
  /CMakeLists\\.txt\\.user
  /private/
)
include(CPack) # Put this last!

