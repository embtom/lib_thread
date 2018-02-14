project(lib_thread_FREERTOS)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "*.c")

include_directories(${lib_log_SOURCE_DIR})

#Generate the static library from the sources
add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME} itf_lib_convention lib_log lib_FREERTOS itf_lib_thread)

target_include_directories(${PROJECT_NAME} PUBLIC ${itf_lib_convention_SOURCE_DIR} ${itf_lib_thread_SOURCE_DIR})
