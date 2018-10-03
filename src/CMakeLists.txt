
######################################################################################
# Global Registration of the available architectures
######################################################################################
FUNCTION(lib_thread_add_architecture)
FOREACH(ARG ${ARGV})
  set_property(GLOBAL APPEND PROPERTY LIB_THREAD_ARCHITECTURES ${ARG})
ENDFOREACH(ARG)
ENDFUNCTION(lib_thread_add_architecture)

######################################################################################
# Add architecture specific source files  
######################################################################################
FUNCTION(lib_thread_add_sourcefile_c)
  FOREACH(ARG ${ARGV})
    SET_PROPERTY(GLOBAL APPEND PROPERTY LIB_THREAD_ARCH_SOURCE_C ${CMAKE_CURRENT_SOURCE_DIR}/${ARG})
  ENDFOREACH(ARG)
ENDFUNCTION(lib_thread_add_sourcefile_c)

######################################################################################
# Add architecture specific private definitions
######################################################################################
FUNCTION(lib_thread_add_private_definition)
  FOREACH(ARG ${ARGV})
    set_property(GLOBAL APPEND PROPERTY LIB_THREAD_ARCH_PRIVATE_DEFINITION ${ARG})
  ENDFOREACH(ARG)
ENDFUNCTION(lib_thread_add_private_definition)

######################################################################################
# Add architecture specific dependencies
######################################################################################
FUNCTION(lib_thread_add_dependencies)
  FOREACH(ARG ${ARGV})
    set_property(GLOBAL APPEND PROPERTY LIB_THREAD_ARCH_DEPEND ${ARG})
  ENDFOREACH(ARG)
ENDFUNCTION(lib_thread_add_dependencies)


add_subdirectory(posix)
add_subdirectory(freertos)