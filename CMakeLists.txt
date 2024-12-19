cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(PROJECT_NAME "memorandum")
set(PROJECT_VERSION "0.1")

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(MEMORANDUM_BUILD_TESTS "Build tests" ON)

project("${PROJECT_NAME}" 
    VERSION "${PROJECT_VERSION}"
    DESCRIPTION "Small in-memory database"
    LANGUAGES "CXX")

#Compile commands, useful for some IDEs like VS-Code
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CPM)

add_library(${PROJECT_NAME} INTERFACE
    src/memorandum.hpp)

target_include_directories(${PROJECT_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if (MEMORANDUM_BUILD_TESTS)
    CPMAddPackage("gh:catchorg/Catch2@3.7.1")

    set(TEST_SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    set(TEST_SOURCES
    ${TEST_SOURCE_DIR}/01_basic.cpp

    )
    message("test sources = ${TEST_SOURCES}")

    enable_testing()
    add_executable(tests_runner ${TEST_SOURCES})
    target_link_libraries(tests_runner PRIVATE ${PROJECT_NAME} Catch2::Catch2WithMain)

    message("catch2 source = ${Catch2_SOURCE_DIR}")

    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)
    catch_discover_tests(tests_runner)
    #add_test(NAME "ALL TESTS" COMMAND tests_runner.exe --success)
endif()

add_subdirectory(examples)