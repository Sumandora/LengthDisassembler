cmake_minimum_required(VERSION 3.20)

include_guard()

project(LengthDisassembler)
add_library(LengthDisassembler STATIC "Source/LengthDisassembler.cpp")

target_include_directories(LengthDisassembler PUBLIC "${PROJECT_SOURCE_DIR}/Include")
target_compile_features(LengthDisassembler PRIVATE cxx_std_23)
set_target_properties(LengthDisassembler PROPERTIES CXX_EXTENSIONS OFF)

if (PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory("Example")

    add_test(
        NAME VerifyGeneratedOpcodes
        COMMAND sh -c "./x86_parser/autogen.sh && diff ./x86_parser/generated_thin_table.h ./Source/GeneratedOpcodeTables.h"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

