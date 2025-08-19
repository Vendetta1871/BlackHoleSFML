cmake_minimum_required(VERSION 3.16)
project(BlackHoleSFML)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

set(SOURCES
        main.cpp
        Camera.cpp
        Camera.h
        BlackHole.cpp
        BlackHole.h
        ObjectData.h
        Engine.cpp
        Engine.h
)

add_executable(BlackHoleSFML ${SOURCES})

target_link_libraries(BlackHoleSFML
        SFML::Graphics
        SFML::Window
        SFML::System
        SFML::Audio
        GLEW::GLEW
        glm::glm
        OpenGL::GL
)

add_custom_command(TARGET BlackHoleSFML POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:BlackHoleSFML>/shaders)

