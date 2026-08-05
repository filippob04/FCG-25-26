cmake_minimum_required(VERSION 3.5...4.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for all subprojects")

project(SFMLOpenGLExample LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(FetchContent)

############################
### Library dependencies ###
############################

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
        GIT_TAG 	1.0.3
)
FetchContent_MakeAvailable(glm)

add_library(common INTERFACE)
target_compile_features(common INTERFACE cxx_std_20)
target_compile_options(common INTERFACE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -DGL_SILENCE_DEPRECATION>
  $<$<CXX_COMPILER_ID:MSVC>: /W4>
)

# Find OpenGL
find_package(OpenGL REQUIRED)

#find coregraphics for raw mouse events in macos
if(APPLE)
    find_library(CORE_GRAPHICS_LIB CoreGraphics REQUIRED)
endif()

###############################
### Sources and executables ###
###############################

SET( fn00 "00_game" )
add_executable( ${fn00} src/00/${fn00}.cpp )
target_link_libraries( ${fn00} PRIVATE common SFML::Window OpenGL::GL glm::glm-header-only ${CORE_GRAPHICS_LIB})
target_include_directories( ${fn00} PRIVATE ${CMAKE_SOURCE_DIR}/resources/include )

SET( fn01 "01_game" )
add_executable( ${fn01} src/01/${fn01}.cpp )
target_link_libraries( ${fn01} PRIVATE common SFML::Window OpenGL::GL glm::glm-header-only ${CORE_GRAPHICS_LIB})
target_include_directories( ${fn01} PRIVATE ${CMAKE_SOURCE_DIR}/resources/include )

SET( fn02 "02_game" )
add_executable( ${fn02} src/02/${fn02}.cpp )
target_link_libraries( ${fn02} PRIVATE common SFML::System SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only ${CORE_GRAPHICS_LIB})
target_include_directories( ${fn02} PRIVATE ${CMAKE_SOURCE_DIR}/resources/include )
