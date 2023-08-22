cmake_minimum_required(VERSION 3.21)

project(uexec VERSION 3.0.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 99)

include_directories(.)

set(UEXEC_LIBRARY_TYPE "SHARED")
if (BUILD_VARIANT_STATIC)
    set(UEXEC_LIBRARY_TYPE "STATIC")
endif()

set(UEXEC_HEADERS "uexec.hpp" "uexecdata.h" "uexecunix.hpp" "uexecwindows.hpp")
set(UEXEC_C_HEADERS "C/cuexec.h")

add_library(uexec ${UEXEC_LIBRARY_TYPE} uexec.cpp uexecunix.cpp uexecwindows.cpp C/cuexec.cpp ${UEXEC_HEADERS} ${UEXEC_C_HEADERS})
if (NOT WIN32)
    target_link_libraries(uexec util)
endif()

target_compile_definitions(uexec PUBLIC UIMGUI_UEXEC_SUBMODULE_ENABLED)
list(APPEND compile_defs "UIMGUI_UEXEC_SUBMODULE_ENABLED")

configure_file(uexec.pc.in uexec.pc @ONLY)

if (UIMGUI_INSTALL)
    install(TARGETS uexec
            LIBRARY DESTINATION lib/
            ARCHIVE DESTINATION lib/)
    install(FILES ${UEXEC_HEADERS} DESTINATION include/uexec)
    install(FILES ${UEXEC_C_HEADERS} DESTINATION include/uexec/C)
    install(FILES ${PROJECT_BINARY_DIR}/uexec.pc
            DESTINATION lib/pkgconfig/)
endif()
