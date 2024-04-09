function(make_test)
    set(options FULL SIMPLE)
    set(single_value_args NAME ENABLE_MATCH TIMEOUT)
    set(multi_value_args SOURCES ENV_VARS)
    cmake_parse_arguments(
        MAKE_TEST
        "${options}"
        "${single_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(MAKE_TEST_FULL AND "${MAKE_TEST_ENABLE_MATCH}" STREQUAL "")
        message(FATAL_ERROR "\"FULL\" tests require the \"ENABLE_MATCH\" argument")
    endif()

    if(MAKE_TEST_SIMPLE)
        set(TEST_ENABLED ON)
    elseif(MAKE_TEST_FULL AND "${${MAKE_TEST_ENABLE_MATCH}}" STREQUAL "FULL")
        set(TEST_ENABLED ON)
    endif()

    if (NOT TEST_ENABLED)
        return()
    endif()
    add_executable(${MAKE_TEST_NAME} ${MAKE_TEST_SOURCES})
    target_link_libraries(${MAKE_TEST_NAME} PRIVATE testing exios)
    add_test(NAME ${MAKE_TEST_NAME} COMMAND ${MAKE_TEST_NAME})
    set_tests_properties(
        ${MAKE_TEST_NAME}
        PROPERTIES
            ENVIRONMENT "${MAKE_TEST_ENV_VARS}"
    )

    if("${MAKE_TEST_TIMEOUT}" STREQUAL "")
        set(MAKE_TEST_TIMEOUT 2)
    endif()

    set_tests_properties(
        ${MAKE_TEST_NAME}
        PROPERTIES
            TIMEOUT ${MAKE_TEST_TIMEOUT}
    )
endfunction()
