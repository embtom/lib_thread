project(lib_thread)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "*.c")

set(TREADING_LIBS pthread rt) 

add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME} itf_lib_convention itf_lib_thread lib_log)
target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE=1 __USE_GNU TRACE)
target_link_libraries(${PROJECT_NAME} ${TREADING_LIBS} )
