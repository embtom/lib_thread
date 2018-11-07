lib_thread_add_architecture("posix")

if("${LIB_THREAD_ARCHITECTURE}" STREQUAL "posix")
    lib_thread_add_sourcefile_c(lib_thread_POSIX.c lib_thread_wakeup_POSIX.c)
    lib_thread_add_private_definition(_GNU_SOURCE=1 __USE_GNU)
    lib_thread_add_dependencies(lib_timer)
endif()
