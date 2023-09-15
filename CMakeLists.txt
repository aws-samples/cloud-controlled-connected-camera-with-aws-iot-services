cmake_minimum_required(VERSION 3.12)
# Cloud-Controlled-Connected-camera
project(c3-camera VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)

# If all you need is to get the source code of a library, include in your CMake project, 
# and have it compiled from source with the rest of your project, 
# CMake’s FetchContent module can do it for you.
# https://cmake.org/cmake/help/latest/module/FetchContent.html
include(FetchContent)

set(CMAKE_VERBOSE_MAKEFILE TRUE)
# note: cxx-17 requires cmake 3.8, cxx-20 requires cmake 3.12
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

# pass custom ca cert location to webrtc sdk
# add_definitions(-DKVS_CA_CERT_PATH="${CMAKE_SOURCE_DIR}/certs/cert.pem")
# add_definitions(-DCMAKE_DETECTED_CACERT_PATH)

#########################################################################
FetchContent_Declare(
        producercpp
        GIT_REPOSITORY https://github.com/awslabs/amazon-kinesis-video-streams-producer-sdk-cpp.git
        GIT_TAG        v3.4.0
)

# git name-rev 15e60193456709a3786f7c0f237c49ea7bd9c81f
FetchContent_Declare(
  webrtc
  GIT_REPOSITORY https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c
  GIT_TAG        v1.8.1
)

# git show-ref v1.27.0 3d66ef32c27143ab84403b6c4179913686fd25b0
FetchContent_Declare(
        devicecpp
        GIT_REPOSITORY https://github.com/aws/aws-iot-device-sdk-cpp-v2.git
        GIT_TAG        v1.27.0
)

FetchContent_GetProperties(producercpp)
if(NOT producercpp_POPULATED)
  FetchContent_Populate(producercpp)
  add_subdirectory(${producercpp_SOURCE_DIR} ${producercpp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_GetProperties(webrtc)
if(NOT webrtc_POPULATED)
  FetchContent_Populate(webrtc)
  add_subdirectory(${webrtc_SOURCE_DIR} ${webrtc_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_GetProperties(devicecpp)
if(NOT devicecpp_POPULATED)
  FetchContent_Populate(devicecpp)
  add_subdirectory(${devicecpp_SOURCE_DIR} ${devicecpp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

message(STATUS "KVS C Source dir: ${KinesisVideoProducerC_SOURCE_DIR}")
file(GLOB producerc_HEADERS "${KinesisVideoProducerC_SOURCE_DIR}/src/include")
file(GLOB PIC_HEADERS "${pic_project_SOURCE_DIR}/src/*/include")

#########################################################################
# amazon-kinesis-video-streams-producer-sdk
include_directories(${PIC_HEADERS})
include_directories(${producerc_HEADERS})
include_directories(${producercpp_SOURCE_DIR}/src)
include_directories(${producercpp_SOURCE_DIR}/src/credential-providers/)
include_directories(${producercpp_SOURCE_DIR}/open-source/include)
link_directories(${producercpp_SOURCE_DIR}/open-source/lib)

# amazon-kinesis-video-streams-webrtc-sdk-c
include_directories(${webrtc_SOURCE_DIR}/src/include)
include_directories(${webrtc_SOURCE_DIR}/open-source/include)
link_directories(${webrtc_SOURCE_DIR}/open-source/lib)

# aws-iot-device-sdk
include_directories(${devicecpp_SOURCE_DIR}/crt/aws-crt-cpp/include)
include_directories(${devicecpp_SOURCE_DIR}/shadow/include)

find_package(PkgConfig) # pkg-config is a helper tool
find_package(Log4cplus REQUIRED)

#########################################################################
#using pkg-config to getting Gstreamer and Gstreamer-app
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GOBJ2 REQUIRED gobject-2.0)
#including GStreamer header files directory
include_directories(
        ${GLIB_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
        ${GST_APP_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
        ${GOBJ2_INCLUDE_DIRS}
        ${LOG4CPLUS_INCLUDE_DIR}
)
#linking GStreamer and Gstreamer-app library directory
link_directories(
        ${GLIB_LIBRARY_DIRS}
        ${GSTREAMER_LIBRARY_DIRS}
        ${GST_APP_LIBRARY_DIRS}
        ${GLIB2_LIBRARY_DIRS}
        ${GOBJ2_LIBRARY_DIRS}
)

#########################################################################
# kvssink
file(GLOB GST_PLUGIN_SOURCE_FILES
        "${producercpp_SOURCE_DIR}/src/gstreamer/*.cpp"
        "${producercpp_SOURCE_DIR}/src/gstreamer/Util/*.cpp"
)
add_library(gstkvssink MODULE ${GST_PLUGIN_SOURCE_FILES})
target_link_libraries(gstkvssink PRIVATE ${GST_APP_LIBRARIES} KinesisVideoProducer)

#########################################################################
# webrtc
add_library(c3webrtc
        source/Servo.cpp
        source/DeviceManager.cpp
        source/utils/CommandLineUtils.cpp
        source/WebRtcCommon.cpp
        source/WebRtcSink.cpp
)

target_link_libraries(c3webrtc
        pigpio 
        ${GSTREAMER_LIBRARIES} ${LOG4CPLUS_LIBRARIES}
        ${GST_APP_LIBRARIES}
        kvsWebrtcClient
        kvsWebrtcSignalingClient
        kvspicUtils
        aws-crt-cpp
        IotShadow-cpp
)

add_executable(${PROJECT_NAME}-webrtc
        source/C3CameraWebrtc.cpp 
)
target_link_libraries(
        ${PROJECT_NAME}-webrtc
        c3webrtc
)

#########################################################################
# producer
add_library(c3producer
        source/Servo.cpp
        source/DeviceManager.cpp
        source/utils/CommandLineUtils.cpp
        source/ProducerSink.cpp
)

target_link_libraries(c3producer
        pigpio 
        ${GSTREAMER_LIBRARIES} ${LOG4CPLUS_LIBRARIES}
        ${GST_APP_LIBRARIES}
        kvsWebrtcClient
        kvsWebrtcSignalingClient
        kvspicUtils
        aws-crt-cpp
        IotShadow-cpp
)

add_executable(${PROJECT_NAME}-producer
        source/C3CameraProducer.cpp 
)
target_link_libraries(
        ${PROJECT_NAME}-producer
        c3producer
)