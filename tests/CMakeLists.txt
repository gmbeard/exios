include(MakeTest)
add_library(testing OBJECT testing.cpp)

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
