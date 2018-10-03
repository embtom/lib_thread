

lib_thread_add_architecture("freertos")

if("${LIB_THREAD_ARCHITECTURE}" STREQUAL "freertos")
    lib_thread_add_sourcefile_c(lib_thread_FREERTOS.c lib_thread_wakeup_FREERTOS.c)
    lib_thread_add_dependencies(lib_list lib_FREERTOS)
endif()


#message(STATUS "##### ${LIB_THREAD_ARCHITECTURE}")
# project(lib_thread)

# #However, the file(GLOB...) allows for wildcard additions:
# SET(SOURCES lib_thread__FREERTOS.c lib_thread_wakeup_FREERTOS.c)

# if(${MCU} STREQUAL "cortex-m3")
#     add_definitions(-DCORTEX_M3) 
# ENDIF()

# if(${MCU} STREQUAL "cortex-m4")
#     add_definitions(-DCORTEX_M4) 
# ENDIF()


# #Generate the static library from the sources
# add_library(${PROJECT_NAME} STATIC ${SOURCES})
# target_link_libraries(${PROJECT_NAME} itf_lib_convention lib_log lib_FREERTOS lib_list itf_lib_thread)

#target_include_directories(${PROJECT_NAME} PUBLIC ${itf_lib_convention_SOURCE_DIR} ${itf_lib_thread_SOURCE_DIR})
