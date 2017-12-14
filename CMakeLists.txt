project(lib_thread)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "*.c")

add_definitions(-D_GNU_SOURCE=1 -D__USE_GNU)
add_definitions(-DTRACE)

include_directories(${lib_log_SOURCE_DIR})

#Generate the static library from the sources
add_library(${PROJECT_NAME} OBJECT ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${itf_lib_convention_SOURCE_DIR} ${itf_lib_thread_SOURCE_DIR})
