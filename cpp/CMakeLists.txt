cmake_minimum_required(VERSION 3.23)
project(rutherford_project)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(includes/glfw)
add_subdirectory(includes/glm)

add_executable(project 
src/main.cpp
src/shaders.cpp
)

add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/includes/glad/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/includes/glad)

target_include_directories(project PRIVATE headers)

target_link_libraries(project PRIVATE glfw glad glm::glm)