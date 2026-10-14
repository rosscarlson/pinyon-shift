cmake_minimum_required(VERSION 3.25)
project(pinyon_wheel_setup LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: the EXE runs on a PC without the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# The same SDL3 source and joystick defaults the game's SDK builds, so device
# names, paths and axis/button numbers here are the ones the game sees.
set(SHIFTGLUE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/shiftglue-sdk" CACHE PATH "ShiftGlue SDK checkout")
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI_LIBUSB OFF CACHE BOOL "" FORCE)
set(SDL_RENDER ON CACHE BOOL "" FORCE)
set(SDL_HAPTIC ON CACHE BOOL "" FORCE)
set(SDL_DIALOG ON CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
add_subdirectory("${SHIFTGLUE_SDK}/thirdparty/sdl3" sdl3 EXCLUDE_FROM_ALL)

add_executable(PinyonWheelSetup WIN32 main.cpp)
target_link_libraries(PinyonWheelSetup PRIVATE SDL3::SDL3-static)
