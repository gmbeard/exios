include(MakeTest)
add_library(testing OBJECT testing.cpp)

make_test(
    NAME async_operation_tests
    SOURCES async_operation_tests.cpp
    SIMPLE
)
