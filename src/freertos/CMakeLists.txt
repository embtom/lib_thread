lib_thread_add_architecture("freertos")

if("${LIB_THREAD_ARCHITECTURE}" STREQUAL "freertos")
    lib_thread_add_sourcefile_c(lib_thread_FREERTOS.c lib_thread_wakeup_FREERTOS.c)
    lib_thread_add_dependencies(lib_list lib_FREERTOS)
endif()
