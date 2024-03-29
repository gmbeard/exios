include(MakeTest)
add_library(testing OBJECT testing.cpp)

if(NOT EXIOS_TEST_TYPE)
    set(EXIOS_TEST_TYPE "SIMPLE")
endif()

make_test(
    NAME async_operation_tests
    SOURCES async_operation_tests.cpp
    SIMPLE
)

make_test(
    NAME timer_tests
    SOURCES timer_tests.cpp
    SIMPLE
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
    SIMPLE
)

make_test(
    NAME cancellation_tests
    SOURCES cancellation_tests.cpp
    SIMPLE
)

make_test(
    NAME signal_tests
    SOURCES signal_tests.cpp
    FULL
    ENABLE_MATCH EXIOS_TEST_TYPE
)
