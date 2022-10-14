cmake_minimum_required(VERSION 3.23)

project(CppEasySerDes VERSION 1.0.0.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CPP_EASY_SERDES_BuildTests "Build an executable that generates an example trace file." ON)
option(CPP_EASY_SERDES_RunTests "Build an executable that generates an example trace file." OFF)

include(FetchContent)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
include(json)
include(catch2)

add_library(CppEasySerDes INTERFACE
    EasySerDes.h
    EasySerDesCore.h
    EasySerDesClassHelper.h
    EasySerDesBuiltInTypeSupport.h
    EasySerDesStdLibSupport.h
    EasySerDesJsonHelpers.h
)

add_custom_target(Other
    SOURCES
    README.md
)

#target_link_libraries(CppEasySerDes PRIVATE
target_link_libraries(CppEasySerDes INTERFACE
    nlohmann_json::nlohmann_json
)

if (CPP_EASY_SERDES_BuildTests)
    add_subdirectory(test)
endif()
