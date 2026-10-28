# as you read this file you will begin to understand the hell that was the windows port T-T

cmake_minimum_required(VERSION 3.20)

project(sourcebox C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# this is just for windows to be happy. linux sees that it doesnt include the c unit files and uses the 
# system package of sdl2. Windows needs to use precompiled libs from MINGW, which means that this probably 
# doesnt compile on anything other than MINGW64. cool T-T
set(SOURCEBOX_SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sdl2"
    CACHE PATH "Root of the vendored SDL2 development tree")

if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "${SOURCEBOX_SDL2_ROOT}")
endif()

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_image REQUIRED CONFIG)
find_package(SDL2_mixer REQUIRED CONFIG)
find_package(SDL2_ttf REQUIRED CONFIG)

# vendored glew, i only compile glew.c
# could probably use the system glew, but windows is once again a bitch about it

add_library(sb_glew STATIC glew/src/glew.c)
target_include_directories(sb_glew SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/glew/include")
# linux workflow fails without setting GLEW_NO_GLU here, but on my real machine it didnt fail? weird ubuntu-latest behaviour?
target_compile_definitions(sb_glew PUBLIC GLEW_STATIC GLEW_NO_GLU)
target_link_libraries(sb_glew PUBLIC OpenGL::GL)

# this is for x11 linkage, libdl is sometimes merged into libc, so this does 
# nothing on those systems. windows isnt affected by this (THANK GOD)
target_link_libraries(sb_glew PUBLIC ${CMAKE_DL_LIBS})

# glew.c is recommended to compile with no warnings or as little of them as possible (it is a huge 600kb file)
if(MSVC)
    target_compile_options(sb_glew PRIVATE /w)
else()
    target_compile_options(sb_glew PRIVATE -w)
endif()

# vendored libGLU snipped from Mesa, because compiling mesa on its entirety would make me do unspeakable 
# things on windows. also i only use 7 functions so i was considering on rewriting those without 
# using GLU but i decided i was insane enough already :)
add_library(sb_glu STATIC
    libglu/src/libutil/project.c
    libglu/src/libutil/quad.c
    libglu/src/libutil/registry.c
    libglu/src/libutil/error.c)

target_include_directories(sb_glu SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/glvnd/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libglu/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libglu/src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libglu/src/libutil")

# BUILD_GLU32 enables the correct definitions for glu to build statically, if not it 
# tries to dllimport or dllexport on windows and causes some warnings 
# (dllexport from binary that is gonna get linked into the .exe file)
target_compile_definitions(sb_glu PRIVATE BUILD_GLU32)

# some mesa code triggers some warnings when only compiling glu, so suppress those
# I LIED!! the project compiles with SOME warnings but they arent from my code and i cant remedy them otherwise...
if(MSVC)
    target_compile_options(sb_glu PRIVATE /w)
else()
    target_compile_options(sb_glu PRIVATE -Wno-stringop-overflow)
endif()

target_link_libraries(sb_glu PUBLIC OpenGL::GL)

add_executable(sourcebox
    src/main.c

    src/core/sb_log.c
    src/core/sb_platform.c
    src/core/sb_resources.c
    src/core/sb_rng.c

    src/audio/sb_audio.c

    src/gl/sb_image.c
    src/gl/sb_text.c

    src/render/sb_cursor.c
    src/render/sb_main_scene.c
    src/render/sb_raycast.c

    src/scenes/sb_cone_entities.c
    src/scenes/sb_cone_scene.c
    src/scenes/sb_cone_renderer.c
    src/scenes/sb_missing_texture.c)

target_include_directories(sourcebox PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# compiling with -mwindows causes SDL2 to error out when compiling on windows for its custom main function
# the solution? trick the linker to run over SDL2 twice. the jank is through the roof
set(SB_SDL2_TARGETS
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf)

if(TARGET SDL2::SDL2main)
    list(INSERT SB_SDL2_TARGETS 1 SDL2::SDL2main)
endif()

target_link_libraries(sourcebox PRIVATE
    sb_glew
    sb_glu
    ${SB_SDL2_TARGETS})

if(WIN32)
    get_target_property(SB_SDL2_IMPLIB SDL2::SDL2 IMPORTED_IMPLIB)
    if(SB_SDL2_IMPLIB)
        target_link_libraries(sourcebox PRIVATE "${SB_SDL2_IMPLIB}")
    else()
        message(FATAL_ERROR
            "SDL2::SDL2 exposes no IMPORTED_IMPLIB; linker wont be able to pass twice over SDL2 :(")
    endif()
endif()

if(MSVC)
    target_compile_options(sourcebox PRIVATE /W3)
else()
    target_compile_options(sourcebox PRIVATE -Wall -Wextra -Wno-unused-parameter)

    # glibc is a bitch and hides some functions if i dont ask for the default set
    if(NOT WIN32)
        target_compile_definitions(sourcebox PRIVATE _DEFAULT_SOURCE)
    endif()

    # glibc has the math code in a separate library.
    find_library(SB_LIBM m)
    if(SB_LIBM)
        target_link_libraries(sourcebox PRIVATE ${SB_LIBM})
    endif()
endif()

# hide cmd window on Windows, although this makes it so that i cant log to stdout normally (see code for more) :(
if(MINGW)
    target_link_options(sourcebox PRIVATE -mwindows)
endif()

# copy the SDL2.dll files to the build folder because windows needs them in the cwd
if(WIN32)
    add_custom_command(TARGET sourcebox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOURCEBOX_SDL2_ROOT}/bin/SDL2.dll"
                "${SOURCEBOX_SDL2_ROOT}/bin/SDL2_image.dll"
                "${SOURCEBOX_SDL2_ROOT}/bin/SDL2_mixer.dll"
                "${SOURCEBOX_SDL2_ROOT}/bin/SDL2_ttf.dll"
                "$<TARGET_FILE_DIR:sourcebox>"
        COMMENT "Copying SDL2 runtime DLLs next to the executable")
endif()

install(TARGETS sourcebox RUNTIME DESTINATION bin)
install(DIRECTORY assets DESTINATION share/sourcebox)
