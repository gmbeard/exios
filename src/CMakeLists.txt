add_library(
    exios

    async_io_operation.cpp
    async_operation.cpp
    buffer_view.cpp
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
    ssl.cpp
    ssl_state.cpp
    tcp_socket.cpp
    timer.cpp
    tls_operations.cpp
    tls_tcp_socket.cpp
    udp_socket.cpp
    unix_socket.cpp
    utils.cpp
)

add_library(Exios::exios ALIAS exios)

target_include_directories(
    exios
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    exios
    PUBLIC
    OpenSSL::SSL
)
