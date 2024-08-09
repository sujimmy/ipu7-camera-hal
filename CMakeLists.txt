#
#  Copyright (C) 2017-2022 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

cmake_minimum_required(VERSION 2.8)

project(libcamhal)


#---------------------------- Fold settings ----------------------------

# Source directories
set(SRC_ROOT_DIR        ${CMAKE_CURRENT_LIST_DIR}/src)

set(3A_DIR              ${SRC_ROOT_DIR}/3a)
set(CORE_DIR            ${SRC_ROOT_DIR}/core)
set(HAL_DIR             ${SRC_ROOT_DIR}/hal)
set(IIO_DIR             ${SRC_ROOT_DIR}/iio)
set(IUTILS_DIR          ${SRC_ROOT_DIR}/iutils)
set(METADATA_DIR        ${SRC_ROOT_DIR}/metadata)
set(PLATFORMDATA_DIR    ${SRC_ROOT_DIR}/platformdata)
set(SCHEDULER_DIR       ${SRC_ROOT_DIR}/scheduler)
set(V4L2_DIR            ${SRC_ROOT_DIR}/v4l2)
set(MODULES_DIR         ${CMAKE_CURRENT_LIST_DIR}/modules)
set(ALGOWRAPPER_DIR     ${MODULES_DIR}/algowrapper)
set(SANDBOXING_DIR      ${MODULES_DIR}/sandboxing)
set(IMAGE_PROCESS_DIR   ${SRC_ROOT_DIR}/image_process)

if (NOT ${IPU_VERSIONS})
    set(${IPU_VERSIONS} ${IPU_VER})
endif()

if (WITH_HAL_ADAPTOR)
    add_subdirectory(src/hal/hal_adaptor)
endif()

foreach(IPU_VER ${IPU_VERSIONS})
set(TARGET_SUFFIX "-${IPU_VER}")

#AAL directories
if (CAL_BUILD)
    set(AAL_ROOT_DIR       ${CMAKE_CURRENT_LIST_DIR}/aal)
    set(JPEG_DIR           ${SRC_ROOT_DIR}/jpeg)
    set(MEMORY_DIR         ${MODULES_DIR}/memory)
endif() #CAL_BUILD

if (SUPPORT_LIVE_TUNING)
    #LiveTuning directories
    set(LIVE_TUNING_DIR         ${MODULES_DIR}/livetune)
endif() #SUPPORT_LIVE_TUNING

# CMake module directory
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Install directories
# CMAKE_INSTALL_PREFIX is defined when executing cmake and supposed to be ./out/libcamhal
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}" CACHE PATH "Target location for install command." FORCE)
endif()

message(STATUS "install prefix: " ${CMAKE_INSTALL_PREFIX})

#------------------------- Compilation settings -------------------------

include(GNUInstallDirs)

# -fPIC in xos way
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-faligned-new COMPILER_SUPPORTS_ALIGNED_NEW)
if (COMPILER_SUPPORTS_ALIGNED_NEW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -faligned-new")
endif()

set (CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall
                    -fstack-protector
                    -fPIE -fPIC
                    -DDCHECK_ALWAYS_ON
                    -Wformat -Wformat-security
                    )
# code coverage.
include(code-coverage)

add_definitions(-D__STDC_FORMAT_MACROS
                -DHAVE_PTHREADS
                -DHAVE_LINUX_OS
                -DHAVE_IA_TYPES
                -DHAVE_PRCTL
                )

if (USE_PG_LITE_PIPE)
        add_definitions(-DUSE_PG_LITE_PIPE)
endif() #USE_PG_LITE_PIPE

if (CAL_BUILD)
    message("Chrome enabled ")
    add_definitions(-DCAL_BUILD)
    add_definitions(-DDUMP_IMAGE)
else()
    add_definitions(-DSUPPORT_MULTI_PROCESS)
    add_definitions(-DLINUX_BUILD)
    add_definitions("-DCAMERA_DEFAULT_CFG_PATH=\"${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER}/\"")
endif() #CAL_BUILD

if (FACE_DETECTION)
    add_definitions(-DFACE_DETECTION)
endif() #FACE_DETECTION

# IPU6_FEATURE_S
if (IPU_VER MATCHES "ipu6")
    add_definitions(-DIPU_SYSVER_IPU6)
endif()
# IPU6_FEATURE_E

# IPU6_SE_FEATURE_S
if (IPU_VER MATCHES "ipu6sepla")
    add_definitions(-DIPU_SYSVER_ipu6v3)
endif()
# IPU6_SE_FEATURE_E

if (IPU_VER MATCHES "ipu6ep")
    message(INFO "add definition -DIPU_SYSVER_ipu6v5 for ipu6ep")
    add_definitions(-DIPU_SYSVER_ipu6v5)
endif()

if (IPU_VER MATCHES "ipu7")
    message(STATUS "add definition -DIPU_SYSVER_ipu7 for ipu7")
    add_definitions(-DIPU_SYSVER_ipu7 -DSTATIC_GRAPH_USE_IA_AIC_TYPES -DPAC_ENABLE)
endif()

include_directories(include
                    include/api include/utils
                    ${CMAKE_CURRENT_LIST_DIR}
                    src
                    src/v4l2 src/core src/metadata
                    src/platformdata src/platformdata/gc
                    src/3a src/3a/intel3a src/3a/external
                    src/fd
                    src/image_process
                    )
if (IPU_VER MATCHES "ipu7")
include_directories(src/core/processingUnit
                    src/scheduler
                    src/jpeg
                    src/iutils
                    )
else ()
include_directories(src/core/psysprocessor)
endif()

set(LIBCAMHAL_LD_FLAGS "-fPIE -fPIC -Wformat -Wformat-security -Wl,-z,relro -Wl,-z,now")

# check if _FORTIFY_SOURCE is default defined with -O
include(CheckFortifyLevel)
set(FORTIFY_SOURCE_VALUE)
check_fortify_source(FORTIFY_SOURCE_VALUE)
if(FORTIFY_SOURCE_VALUE)
    message(STATUS "compiler default _FORTIFY_SOURCE=${FORTIFY_SOURCE_VALUE}")
else()
    message(STATUS "_FORTIFY_SOURCE is not defined or could not be determined.")
    add_compile_options(-D_FORTIFY_SOURCE=2)
    set(LIBCAMHAL_LD_FLAGS "${LIBCAMHAL_LD_FLAGS} -D_FORTIFY_SOURCE=2")
    message(STATUS "define _FORTIFY_SOURCE=2")
endif()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LIBCAMHAL_LD_FLAGS}")

add_subdirectory(src)
add_subdirectory(modules)

# Set source files
if (CAL_BUILD)
    if (SW_JPEG_ENCODE)
        add_definitions(-DSW_JPEG_ENCODE)
        include_directories(src/jpeg/sw)
    else ()
        include_directories(src/jpeg/chrome)
    endif()

    if (SW_POST_PROCESSING)
        include_directories(src/image_process/sw)
    else ()
        include_directories(src/image_process/chrome)
    endif()

    add_subdirectory(aal)

    set(LIBCAMHAL_SRCS
        ${LIBCAMHAL_SRCS}
        ${MEMORY_SRCS}
        ${AAL_SRCS}
        ${JPEG_SRCS}
        )
    include_directories(include include/api include/utils include/linux
                        ${USR_INCLUDE_HEADER}/android/system/core/include/
                        ${USR_INCLUDE_HEADER}/ia_imaging
                        ${USR_INCLUDE_HEADER}/lib${IPU_VER}
                        ${USR_INCLUDE_HEADER}/android/hardware/libhardware/include/
                        aal aal/chrome src/iutils src/jpeg
                        modules/memory modules/memory/chrome
                        )
endif() #CAL_BUILD

if (SUPPORT_LIVE_TUNING)
    set(LIBCAMHAL_SRCS ${LIVE_TUNING_SRCS})
    include_directories(modules/livetune)
endif() #SUPPORT_LIVE_TUNING

set(LIBCAMHAL_SRCS
    ${LIBCAMHAL_SRCS}
    ${3A_SRCS}
    ${CORE_SRCS}
    ${HAL_SRCS}
    ${IPS_CTRL_SRCS}
    ${IUTILS_SRCS}
    ${METADATA_SRCS}
    ${PLATFORMDATA_SRCS}
    ${SCHEDULER_SRCS}
    ${V4L2_SRCS}
    ${ALGOWRAPPER_SRCS}
    ${IMAGE_PROCESS_SRCS}
    )

if(FACE_DETECTION)
set(LIBCAMHAL_SRCS
    ${LIBCAMHAL_SRCS}
    ${FACE_DETECTION_SRCS}
    )
endif() #FACE_DETECTION

if(ENABLE_SANDBOXING)
set(LIBCAMHAL_SRCS
    ${LIBCAMHAL_SRCS}
    ${SANDBOXING_CLIENT_SRCS}
    )
endif() #ENABLE_SANDBOXING

    set(LIBCAMHAL_SRCS ${LIBCAMHAL_SRCS} ${IIO_SRCS})

if (USE_STATIC_GRAPH)
    add_compile_options(-fpermissive -Wno-unused-variable -Wno-maybe-uninitialized)
    include_directories(modules/ipu_desc)

    if (STATIC_GRAPH_IPU_PREFIXS)
        if (STATIC_GRAPH_IPU_PREFIXS MATCHES "Ipu7x")
            set(PRODUCTION_NAME "lnl")
            add_definitions(-DGRC_IPU7X)
        elseif (STATIC_GRAPH_IPU_PREFIXS MATCHES "Ipu75xa")
            set(PRODUCTION_NAME "ptl")
            add_definitions(-DGRC_IPU75XA)
        endif()
        message(STATUS "Use auto generated static graph code, prefix: ${STATIC_GRAPH_IPU_PREFIXS}")
        include_directories(modules/ipu_desc/${PRODUCTION_NAME})
        set(LIBCAMHAL_SRCS ${LIBCAMHAL_SRCS}
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/CBLayoutUtils.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/GraphResolutionConfigurator.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/${STATIC_GRAPH_IPU_PREFIXS}GraphResolutionConfiguratorAutogen.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/${STATIC_GRAPH_IPU_PREFIXS}TerminalDescriptorAutogen.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/${STATIC_GRAPH_IPU_PREFIXS}StaticGraphAutogen.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/${STATIC_GRAPH_IPU_PREFIXS}StaticGraphReaderAutogen.cpp
            )
    else ()
        if (IPU_VER MATCHES "ipu7")
            set(PRODUCTION_NAME "lnl")
        endif ()
        message(STATUS "Use ${PRODUCTION_NAME} static graph code")
        include_directories(modules/ipu_desc/${PRODUCTION_NAME})
        set(LIBCAMHAL_SRCS ${LIBCAMHAL_SRCS}
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/CBLayoutUtils.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/TerminalDescriptorAutogen.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/StaticGraphAutogen.cpp
            ${MODULES_DIR}/ipu_desc/${PRODUCTION_NAME}/StaticGraphReaderAutogen.cpp
            )
    endif () # STATIC_GRAPH_IPU_PREFIXS

endif() # USE_STATIC_GRAPH

# Add libcamhal using the specified sources

if (WITH_PLUGINS)
    set(CAMHAL_TARGET ${IPU_VER}hal)
    set(CAMHAL_STATIC_TARGET ${IPU_VER}hal_static)
else()
    set(CAMHAL_TARGET camhal)
    set(CAMHAL_STATIC_TARGET camhal_static)
endif()

add_library(${CAMHAL_STATIC_TARGET} STATIC ${LIBCAMHAL_SRCS})

if (${CMAKE_VERSION} VERSION_LESS 3.11)
    add_library(${CAMHAL_TARGET} SHARED ${LIBCAMHAL_SRCS})
else()
    add_library(${CAMHAL_TARGET} SHARED $<TARGET_OBJECTS:${CAMHAL_STATIC_TARGET}>)
endif()

set_target_properties(${CAMHAL_TARGET} PROPERTIES OUTPUT_NAME ${CAMHAL_TARGET})
set_target_properties(${CAMHAL_STATIC_TARGET} PROPERTIES OUTPUT_NAME ${CAMHAL_TARGET})

if (WITH_PLUGINS)
    set_target_properties(${CAMHAL_TARGET} PROPERTIES PREFIX "")
    set_target_properties(${CAMHAL_STATIC_TARGET} PROPERTIES PREFIX "")
else()
    set_target_properties(${CAMHAL_TARGET} PROPERTIES SOVERSION 0)
    set_target_properties(${CAMHAL_TARGET} PROPERTIES VERSION "0.0.0")
endif()

#---------------------------- Link settings ----------------------------
target_link_libraries(${CAMHAL_TARGET} ${CMAKE_DL_LIBS})
target_link_libraries(${CAMHAL_STATIC_TARGET} ${CMAKE_DL_LIBS})

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
target_link_libraries(${CAMHAL_TARGET} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${CAMHAL_STATIC_TARGET} ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(${CAMHAL_TARGET} expat rt)
target_link_libraries(${CAMHAL_STATIC_TARGET} expat rt)

if (IPU_VER MATCHES "ipu7")
    target_link_libraries(${CAMHAL_TARGET} jsoncpp)
    target_link_libraries(${CAMHAL_STATIC_TARGET} jsoncpp)
endif()

if (CAL_BUILD)
    target_link_libraries(${CAMHAL_TARGET} camera_v4l2_device)
    target_link_libraries(${CAMHAL_STATIC_TARGET} camera_v4l2_device)
endif()

# Link libraries, the building will be interrupted if libs/includes not found

    # Link ia_imaging
    find_package(IA_IMAGING)
    include_directories(${IA_IMAGING_INCLUDE_DIRS})
    target_link_libraries(${CAMHAL_TARGET} ${IA_IMAGING_LIBS})
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${IA_IMAGING_LIBS})

if (USE_PG_LITE_PIPE)
    find_package(LIBGCSS)
    include_directories(${LIBGCSS_INCLUDE_DIRS})
    target_link_libraries(${CAMHAL_TARGET} ${LIBGCSS_LIBS})
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${LIBGCSS_LIBS})
elseif (NOT USE_STATIC_GRAPH)
    # Link libiacss
    find_package(LIBIACSS)
    include_directories(${LIBIACSS_INCLUDE_DIRS})
    target_link_libraries(${CAMHAL_TARGET} ${LIBIACSS_LIBS})
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${LIBIACSS_LIBS})
endif()

if (USE_PG_LITE_PIPE)
    # Include libipu(4) headers
    find_package(LIBIPU)
    include_directories(${LIBIPU_INCLUDE_DIRS})
    target_link_libraries(${CAMHAL_TARGET} ${IPU_LIB})
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${IPU_LIB})
endif()

if (CAL_BUILD)
    find_package(LIBCAMERA_CLIENT)
    find_package(LIBCAMERA_COMMON)
    find_package(LIBCAMERA_METADATA)
    find_package(LIBCAMERA_V4L2_DEVICE)
    find_package(LIBSYNC)
    find_package(LIBCBM)

    include_directories(${LIBCAMERA_CLIENT_INCLUDE_DIRS} ${LIBCAMERA_COMMON_INCLUDE_DIRS}
                        ${LIBCAMERA_METADATA_INCLUDE_DIRS} ${LIBCAMERA_V4L2_DEVICE_INCLUDE_DIRS}
                        ${LIBSYNC_INCLUDE_DIRS}
                        ${LIBCBM_INCLUDE_DIRS}
                        )
    link_directories(${CMAKE_PREFIX_PATH} ${LIBCAMERA_CLIENT_LIBS} ${LIBCAMERA_METADATA_LIBS}
                     ${LIBCAMERA_COMMON_LIBS} ${LIBSYNC_LIBS}
                     ${LIBCBM_LIBS}
                     )

    target_link_libraries(${CAMHAL_TARGET} ${LIBCAMERA_CLIENT_LIBS} ${LIBCAMERA_COMMON_LIBS}
                          ${LIBCAMERA_METADATA_LIBS} ${LIBCAMERA_V4L2_DEVICE_LIBS}
                          ${LIBSYNC_LIBS} jpeg ${CMAKE_PREFIX_PATH}/libyuv.pic.a
                          ${LIBCBM_LIBS}
                          )
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${LIBCAMERA_CLIENT_LIBS} ${LIBCAMERA_COMMON_LIBS}
                          ${LIBCAMERA_METADATA_LIBS} ${LIBCAMERA_V4L2_DEVICE_LIBS}
                          ${LIBSYNC_LIBS} jpeg ${CMAKE_PREFIX_PATH}/libyuv.pic.a
                          ${LIBCBM_LIBS}
                          )
endif() #CAL_BUILD

if (NOT CAL_BUILD)
    include_directories(${MODULES_DIR}/v4l2)
endif()

if (FACE_DETECTION)
    target_link_libraries(${CAMHAL_TARGET} pvl_eye_detection pvl_face_detection pvl_mouth_detection)
    target_link_libraries(${CAMHAL_STATIC_TARGET} pvl_eye_detection pvl_face_detection pvl_mouth_detection)
    target_link_libraries(${CAMHAL_TARGET} ${WORK_DIR_TMP}/src/fd/libface_detection.a)
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${WORK_DIR_TMP}/src/fd/libface_detection.a)
endif()

if (ENABLE_SANDBOXING)
    add_definitions(-DENABLE_SANDBOXING)

    find_package(LIBMOJO)
    find_package(LIBCAMERA_IPC)
    find_package(LIBCAB)

    include_directories(${USR_INCLUDE_HEADER}/base-${BASEVER})
    include_directories(${LIBMOJO_INCLUDE_DIRS})
    include_directories(${LIBCAMERA_IPC_INCLUDE_DIRS})
    include_directories(${LIBCAB_INCLUDE_DIRS})

    link_directories(${CMAKE_PREFIX_PATH}
                     ${LIBMOJO_LIBS}
                     ${LIBCAMERA_IPC_LIBS}
                     ${LIBCAB_LIBS}
                     )

    target_link_libraries(${CAMHAL_TARGET} ${LIBCAB_LIBS})
    target_link_libraries(${CAMHAL_TARGET} ${LIBMOJO_LIBS})
    target_link_libraries(${CAMHAL_TARGET} ${LIBCAMERA_IPC_LIBS})
    target_link_libraries(${CAMHAL_TARGET} ${CMAKE_PREFIX_PATH}/librt.a)

    target_link_libraries(${CAMHAL_STATIC_TARGET} ${LIBCAB_LIBS})
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${LIBMOJO_LIBS})
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${LIBCAMERA_IPC_LIBS})
    target_link_libraries(${CAMHAL_STATIC_TARGET} ${CMAKE_PREFIX_PATH}/librt.a)
endif() #ENABLE_SANDBOXING


#--------------------------- Install settings ---------------------------
if (DYNAMIC_PAC_BUFFERS)
    target_compile_definitions(${CAMHAL_TARGET} PUBLIC -DMAX_PAC_BUFFERS=${DYNAMIC_PAC_BUFFERS})
    target_compile_definitions(${CAMHAL_STATIC_TARGET} PUBLIC -DMAX_PAC_BUFFERS=${DYNAMIC_PAC_BUFFERS})
else()
    target_compile_definitions(${CAMHAL_TARGET} PUBLIC -DMAX_PAC_BUFFERS=2U)
    target_compile_definitions(${CAMHAL_STATIC_TARGET} PUBLIC -DMAX_PAC_BUFFERS=2U)
endif()

if (NOT CAL_BUILD)
if (NOT WITH_PLUGINS)
# Install headers
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libcamhal)
if (SUPPORT_LIVE_TUNING)
    install(FILES modules/livetune/LiveTuning.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libcamhal/api)
endif() #SUPPORT_LIVE_TUNING
endif()

# Install configure files
# By default ipu4 is used
if("${IPU_VER}" STREQUAL "ipu6")
    if("${PRODUCTION_NAME}" STREQUAL "Andrews")
        message("Install TGL Andrews configure files")
        install(DIRECTORY config/ubuntu/tgl/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER})
    else()
        message("Install TGL IPU6 configure files")
        install(DIRECTORY config/linux/ipu6/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER})
    endif()
elseif("${IPU_VER}" STREQUAL "ipu6ep")
    if("${PRODUCTION_NAME}" STREQUAL "ccg_platform")
        message("Install ADL Platform configure files")
        install(DIRECTORY config/ubuntu/adl/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER})
    elseif("${PRODUCTION_NAME}" STREQUAL "Tributo")
        message("Install ADL Tributo configure files")
        install(DIRECTORY config/ubuntu/adl/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER})
    else()
        message("Install ADL IPU6EP configure files")
        install(DIRECTORY config/linux/ipu6ep/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER})
    endif()
elseif("${IPU_VER}" STREQUAL "ipu6sepla")
    if("${PRODUCTION_NAME}" STREQUAL "penguin_peak")
        message("Install JSL penguin peak configure files")
        install(DIRECTORY config/ubuntu/jsl/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER})
    endif()
elseif("${IPU_VER}" STREQUAL "ipu7")
    message("Install LNL IPU7 configure files")
    install(DIRECTORY config/linux/ipu7/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER} PATTERN "config/linux/ipu7/gcss/*.xml" EXCLUDE)
else()
    message("Install APL IPU4 configure files")
    install(DIRECTORY config/linux/ipu4/ DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/camera/${IPU_VER})
endif()

# Install libraries

if (WITH_PLUGINS)
    set(CAMHAL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/libcamhal/plugins)
else()
    set(CAMHAL_DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set(CAMHAL_TARGETS ${CAMHAL_TARGET} ${CAMHAL_STATIC_TARGET})
if (${CMAKE_VERSION} VERSION_LESS 3.11)
    install(TARGETS ${CAMHAL_TARGETS}
            LIBRARY DESTINATION ${CAMHAL_DESTINATION}
            ARCHIVE DESTINATION ${CAMHAL_DESTINATION}
            )
else()
    install(TARGETS ${CAMHAL_TARGETS} DESTINATION ${CAMHAL_DESTINATION})
endif()

if(NOT WITH_PLUGINS)
    # Install package config file
    configure_file(${PROJECT_SOURCE_DIR}/cmake/libcamhal.pc.cmakein
                ${PROJECT_SOURCE_DIR}/libcamhal.pc @ONLY)
    install(FILES libcamhal.pc
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

endif() #NOT CAL_BUILD

endforeach()

set(CPACK_GENERATOR "RPM")
include(CPack)
