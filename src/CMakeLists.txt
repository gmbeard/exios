add_library(
    exios

    async_io_operation.cpp
    async_operation.cpp
    context.cpp
    context_thread.cpp
    contracts.cpp
    event.cpp
    file_descriptor.cpp
    io.cpp
    io_object.cpp
    io_scheduler.cpp
    poll_wake_event.cpp
    result.cpp
    signal.cpp
    timer.cpp
    unix_socket.cpp
)

add_library(Exios::exios ALIAS exios)

target_include_directories(
    exios
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
