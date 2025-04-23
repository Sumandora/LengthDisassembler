add_executable(BinaryImporter "Source/Main.cpp")

find_package(Zydis)

target_link_libraries(BinaryImporter PUBLIC Zydis::Zydis)
target_compile_features(BinaryImporter PUBLIC cxx_std_23)
