if (MINGW)
    # This allows Catch2 to be built without MinGW having a fit over the size of the object file generated
    add_compile_options("-Wa,-mbig-obj")
elseif(MSVC)
    add_compile_options("-bigobj")
    set(_FS_C " /FS")
endif()

add_executable(Tests)

target_sources(Tests
    PUBLIC
    main.cpp
    TestBuiltInLanguageTypes.h
    TestBuiltInLanguageTypes.cpp
    TestStdLibrarySupport.cpp
    TestClassHelper.h
    TestClassHelper.cpp
    TestPolymorphismHelper.h
    TestPolymorphismHelper.cpp
    TestJsonHelpers.h
    TestJsonHelpers.cpp
)

target_include_directories(Tests
    PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/Utility
)

target_link_libraries(Tests
    PRIVATE
    Catch2::Catch2
    CppEasySerDes
)

if (BUILD_TESTING AND CPP_EASY_SERDES_RunTests)
    add_custom_command(TARGET Tests POST_BUILD
        COMMAND Tests
        COMMENT "Running Tests..."
    )
endif()
