if(EXIOS_DISABLE_CPPCHECK)
    message(
        STATUS
        "Skipping cppcheck "
        "(EXIOS_DISABLE_CPPCHECK=${EXIOS_DISABLE_CPPCHECK})"
    )
    return()
endif()

find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if(CMAKE_CXX_CPPCHECK)
    message(
        STATUS
        "Enabling cppcheck (${CMAKE_CXX_CPPCHECK})")
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/cppcheck")
    list(APPEND CMAKE_CXX_CPPCHECK
        "--cppcheck-build-dir=${PROJECT_BINARY_DIR}/cppcheck"
        "--inline-suppr"
        "--enable=style,warning"
        "--suppress=noExplicitConstructor"
        "--error-exitcode=1"
        "--check-level=exhaustive"
    )
else()
    message(
        STATUS
        "Skipping cppcheck (${CMAKE_CXX_CPPCHECK})"
    )
    set(CMAKE_CXX_CPPCHECK "")
endif()
