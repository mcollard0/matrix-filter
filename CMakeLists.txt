cmake_minimum_required(VERSION 3.10)
project(matrix-filter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV 4 REQUIRED)
find_package(Freetype REQUIRED)

add_executable(matrix-filter
    src/main.cpp
    src/camera_capture.cpp
    src/virtual_output.cpp
    src/matrix_effect.cpp
    src/static_effect.cpp
)

target_include_directories(matrix-filter PRIVATE ${OpenCV_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(matrix-filter ${OpenCV_LIBS} ${FREETYPE_LIBRARIES})

install(TARGETS matrix-filter RUNTIME DESTINATION bin)
