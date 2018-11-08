project (lib_thread)

cmake_minimum_required(VERSION 2.6)
add_subdirectory(src)

######################################################################################
# Build configurator
######################################################################################

#Selector for logging base
option(LIB_THREAD_EXTERNAL_IOTTY "Usage of the system iotty library for logging" ${LIB_THREAD_EXTERNAL_IOTTY})

#Selector to choose architecture to build lib_thread
set(LIB_THREAD_ARCHITECTURE ${LIB_THREAD_ARCHITECTURE} CACHE STRING "Architecture to build lib_thread on")
get_property(architectures GLOBAL PROPERTY LIB_THREAD_ARCHITECTURES)
list(SORT architectures)
set_property(CACHE LIB_THREAD_ARCHITECTURE PROPERTY STRINGS None ${architectures})

#######################################################################################
# Check LIB_THREAD_ARCHITECTURE has a valid value
#######################################################################################
if("${LIB_THREAD_ARCHITECTURE}" STREQUAL "None")
message(FATAL_ERROR "No valid architecture chosen! Please check LIB_THREAD_ARCHITECTURE.")
endif("${LIB_THREAD_ARCHITECTURE}" STREQUAL "None")

#######################################################################################
# Get Architecture specific build paramters
#######################################################################################
GET_PROPERTY(LIB_THREAD_SOURCE_C            GLOBAL PROPERTY LIB_THREAD_ARCH_SOURCE_C)
GET_PROPERTY(LIB_THREAD_PRIVATE_DEFINITION  GLOBAL PROPERTY LIB_THREAD_ARCH_PRIVATE_DEFINITION)
GET_PROPERTY(LIB_THREAD_DEPEND              GLOBAL PROPERTY LIB_THREAD_ARCH_DEPEND)

if (LIB_THREAD_EXTERNAL_IOTTY)
    find_package(iotty REQUIRED)
    SET(LIB_THREAD_DEPEND ${LIB_THREAD_DEPEND} itf_lib_convention EMBTOM::iotty)
else()
    SET(LIB_THREAD_DEPEND ${LIB_THREAD_DEPEND} itf_lib_convention lib_log)
endif()
SET(LIB_THREAD_PRIVATE_DEFINITION ${LIB_THREAD_PRIVATE_DEFINITION} TRACE )

message(STATUS "LIB_THREAD : selected architecture ${LIB_THREAD_ARCHITECTURE}")

######################################################
#Build target settings
######################################################

add_library(${PROJECT_NAME} STATIC ${LIB_THREAD_SOURCE_C})
target_link_libraries(${PROJECT_NAME} ${LIB_THREAD_DEPEND})
target_include_directories(${PROJECT_NAME} PUBLIC ./include)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${LIB_THREAD_PRIVATE_DEFINITION})
