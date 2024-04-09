include(MakeTest)
add_library(testing OBJECT testing.cpp)

if(NOT EXIOS_TEST_TYPE)
    set(EXIOS_TEST_TYPE "SIMPLE")
endif()

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
    NAME intrusive_multimap_tests
    SOURCES intrusive_multimap_tests.cpp
    TIMEOUT 2
    SIMPLE
)

make_test(
    NAME context_thread_tests
    SOURCES context_thread_tests.cpp
    TIMEOUT 2
    SIMPLE
)
