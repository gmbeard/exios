cmake_minimum_required(VERSION 3.27)

file(READ ".versioning/current" current_version)
string(STRIP "${current_version}" current_version)

project(
    exios
    LANGUAGES CXX
    VERSION ${current_version}
)

if (CMAKE_BUILD_TYPE STREQUAL "")
    message(STATUS "Defaulting CMAKE_BUILD_TYPE to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(CPPCheck)
include(GNUInstallDirs)
include(CheckIPOSupported)
check_ipo_supported(
    RESULT EXIOS_IPO_SUPPORTED
    OUTPUT EXIOS_IPO_SUPORTED_OUTPUT
    LANGUAGES CXX
)

if (EXIOS_IPO_SUPPORTED)
    message(STATUS "IPO supported. Enabling")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
else ()
    message(STATUS "IPO not supported: ${EXIOS_IPO_SUPORTED_OUTPUT}")
endif ()

string(TOLOWER ${CMAKE_CXX_BYTE_ORDER} EXIOS_BYTE_ORDER)

add_compile_options(-Wall -Werror -Wextra -Wshadow)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0)
endif()

option(
    EXIOS_ENABLE_TESTS
    "Enable tests for ${PROJECT_NAME}"
    OFF
)

option(
	EXIOS_ENABLE_ASAN
    "Enable ASan for ${PROJECT_NAME}"
    OFF
)

option(
    EXIOS_ENABLE_TSAN
    "Enable TSan for ${PROJECT_NAME}"
    OFF
)

if (EXIOS_ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

if (EXIOS_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

add_subdirectory(src)

if(EXIOS_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(
    TARGETS exios
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
