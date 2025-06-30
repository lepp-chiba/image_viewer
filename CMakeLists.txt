cmake_minimum_required(VERSION 3.10)
project(Image_Viewer LANGUAGES CXX)

cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE "GLVND")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(TIFF REQUIRED)

set(COMMON_LIBS PRIVATE
    OpenGL::GL
    GLEW::GLEW
    glfw
    TIFF::TIFF
)

add_executable(viewer viewer.cpp)
target_link_libraries(viewer ${COMMON_LIBS})

# shadersディレクトリをビルドディレクトリにコピーする設定
file(COPY shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})