# It's recommended to set a minimum CMake version.
# If you use CMake features from higher versions, update this to match.
cmake_minimum_required(VERSION 3.23)
message("Using toolchain file ${CMAKE_TOOLCHAIN_FILE}.")

########################################################################################################################
## Define project
########################################################################################################################

# Set your project name. This will be the name of your SKSE .dll file.
project(F4SE_HTTP VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

include(cmake/common.cmake)

option(DBUILD_SHARED_LIBS OFF)
option(BUILD_SHARED_LIBS OFF)
option(BUILD_STATIC_CURL ON)
option(BUILD_STATIC_LIBS ON)

set(SOURCES
        src/TypedDictionary.cpp
        src/SKSE_HTTP_TypedDictionary.cpp
        src/plugin.cpp)

source_group(
        TREE ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        ${sources})

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 3b15fa82ea74739b574d705fea44959b58142eb8) # Replace with your desired git commit from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

find_package(fmt REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)

# If you're not using a mod manager, you probably want the SKSE plugin to go
# inside of your Skyrim "Data" folder.
#
# To do this automatically, set the `Fallout4Path` environment variable
# to the path of your Skyrim Special Edition folder
if(DEFINED ENV{Fallout4Path} AND IS_DIRECTORY "$ENV{Fallout4Path}/Data")
    set(OUTPUT_FOLDER "$ENV{Fallout4Path}/Data")
endif()

# If you're using Mod Organizer 2 or Vortex, you might want this to go inside
# of your "mods" folder, inside of a subfolder named "<your mod>".
#
# To do this automatically, set the `MO2_MODS_FOLDER` environment variable
# to the path of your "mods" folder
if(DEFINED ENV{MO2_MODS_FOLDER} AND IS_DIRECTORY "$ENV{MO2_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{MO2_MODS_FOLDER}/${PROJECT_NAME}")
endif()

# Otherwise, you can set OUTPUT_FOLDER to any place you'd like :)
# set(OUTPUT_FOLDER "C:/path/to/any/folder")

########################################################################################################################
## Configure target DLL
########################################################################################################################

add_library(
	"${PROJECT_NAME}"
	SHARED
	${SOURCES}
)

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
		cxx_std_23
)

add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME}
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
        $<INSTALL_INTERFACE:src>)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(${PROJECT_NAME} 
        PRIVATE 
 		fmt::fmt
		spdlog::spdlog
        cpr::cpr)

target_link_libraries(${PROJECT_NAME} 
        PRIVATE
        nlohmann_json::nlohmann_json)

include(cmake/addCommonLibF4NG.cmake)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h) # <--- PCH.h is required!

# When your SKSE .dll is compiled, this will automatically copy the .dll into your mods folder.
# Only works if you configure DEPLOY_ROOT above (or set the MO2_MODS_FOLDER environment variable)
if(DEFINED OUTPUT_FOLDER)
    # If you specify an <OUTPUT_FOLDER> (including via environment variables)
    # then we'll copy your mod files into Skyrim or a mod manager for you!

    # Copy the SKSE plugin .dll files into the F4SE/Plugins/ folder
    set(DLL_FOLDER "${OUTPUT_FOLDER}/F4SE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    # If you perform a "Debug" build, also copy .pdb file (for debug symbols)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()
