add_executable(LengthDisassemblerExample "Source/Main.cpp")

find_package(Zydis)

target_link_libraries(LengthDisassemblerExample PUBLIC LengthDisassembler Zydis::Zydis)
target_compile_features(LengthDisassemblerExample PRIVATE cxx_std_26)

add_test(NAME TestLengthDisassembler COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runner.sh $<TARGET_FILE:LengthDisassemblerExample>)

add_subdirectory("BinaryImporter")
