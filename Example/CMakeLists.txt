add_executable(LengthDisassemblerExample "Source/Main.cpp")

find_package(Zydis)

target_link_libraries(LengthDisassemblerExample PUBLIC LengthDisassembler Zydis::Zydis)
target_compile_features(LengthDisassemblerExample PRIVATE cxx_std_26)

add_test(NAME TestLengthDisassembler COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runner.sh $<TARGET_FILE:LengthDisassemblerExample>)

if(${Zydis_VERSION_MAJOR} GREATER 4 OR (${Zydis_VERSION_MAJOR} EQUAL 4 AND ${Zydis_VERSION_MINOR} GREATER_EQUAL 1))
	add_subdirectory("BinaryImporter")
else()
	message(WARNING "Not building Binary Importer, as Zydis version is older than 4.0.0, got ${Zydis_VERSION}")
endif()
