include(MakeTest)
add_library(testing OBJECT testing.cpp)

if(NOT EXIOS_TEST_TYPE)
    set(EXIOS_TEST_TYPE "SIMPLE")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tmp")

make_test(
    NAME async_operation_tests
    SOURCES async_operation_tests.cpp
    SIMPLE
    TIMEOUT 2
)

make_test(
    NAME timer_tests
    SOURCES timer_tests.cpp
    SIMPLE
    TIMEOUT 2
)

make_test(
    NAME result_tests
    SOURCES result_tests.cpp
    SIMPLE
)

make_test(
    NAME alloc_utils_tests
    SOURCES alloc_utils_tests.cpp
    SIMPLE
)

make_test(
    NAME event_tests
    SOURCES event_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME cancellation_tests
    SOURCES cancellation_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME signal_tests
    SOURCES signal_tests.cpp
    FULL
    ENABLE_MATCH EXIOS_TEST_TYPE
    TIMEOUT 2
)

make_test(
    NAME multi_thread_tests
    SOURCES multi_thread_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME intrusive_list_tests
    SOURCES intrusive_list_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME context_thread_tests
    SOURCES context_thread_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME unix_socket_tests
    SOURCES unix_socket_tests.cpp
    SIMPLE
    ENV_VARS
        "TMP=${CMAKE_CURRENT_BINARY_DIR}/tmp"
)

make_test(
    NAME tcp_socket_tests
    SOURCES tcp_socket_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME udp_socket_tests
    SOURCES udp_socket_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME tls_tests
    SOURCES tls_tests.cpp
    TIMEOUT 2
    SIMPLE
)

#add_test(NAME tls_setup COMMAND ${CMAKE_CURRENT_LIST_DIR}/scripts/initialize_s_server.sh)
#add_test(NAME tls_cleanup COMMAND ${CMAKE_CURRENT_LIST_DIR}/scripts/shutdown_s_server.sh)
#
#set_tests_properties(tls_tests PROPERTIES FIXTURES_REQUIRED tls)
#set_tests_properties(tls_setup PROPERTIES FIXTURES_SETUP tls)
#set_tests_properties(tls_cleanup PROPERTIES FIXTURES_CLEANUP tls)
