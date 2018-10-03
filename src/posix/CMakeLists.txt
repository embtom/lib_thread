
lib_thread_add_architecture("posix")

if("${LIB_THREAD_ARCHITECTURE}" STREQUAL "posix")
    lib_thread_add_sourcefile_c(lib_thread_POSIX.c lib_thread_wakeup_POSIX.c)
    lib_thread_add_private_definition(_GNU_SOURCE=1 __USE_GNU)
    lib_thread_add_dependencies(lib_timer)
endif()




# project(lib_thread)

# #However, the file(GLOB...) allows for wildcard additions:
# file(GLOB SOURCES "*.c")

# message(STATUS "########## ${MACHINE}")

# add_library(${PROJECT_NAME} STATIC ${SOURCES})
# target_link_libraries(${PROJECT_NAME} itf_lib_convention itf_lib_thread lib_log lib_timer)
# target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE=1 __USE_GNU TRACE)

