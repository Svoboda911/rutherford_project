cmake_minimum_required(VERSION 3.23)
project(rutherford_project)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(includes/glfw)
add_subdirectory(includes/glm)

add_executable(project 
src/main.cpp
src/shaders.cpp
)

add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/includes/glad/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/includes/glad)

target_include_directories(project PRIVATE headers)
target_compile_definitions(project PRIVATE SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")
target_compile_options(project PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
    $<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-O3>
)

include(CheckIPOSupported)
check_ipo_supported(RESULT PROJECT_IPO_SUPPORTED OUTPUT PROJECT_IPO_ERROR)
if(PROJECT_IPO_SUPPORTED)
    set_property(TARGET project PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

target_link_libraries(project PRIVATE glfw glad glm::glm)
