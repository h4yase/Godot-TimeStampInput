cmake_minimum_required(VERSION 3.23)

project(TimeStampInput VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
if(BUILD_TESTING)
  add_executable(timestamp_queue_test tests/timestamp_queue_test.cpp)
  target_include_directories(timestamp_queue_test PRIVATE include)
  target_compile_features(timestamp_queue_test PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(timestamp_queue_test PRIVATE /EHsc)
  endif()
  add_test(NAME timestamp_queue_test COMMAND timestamp_queue_test)
endif()

include(FetchContent)

set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Target Godot GDExtension API version")

FetchContent_Declare(
  godot-cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG 9c8aeff0f58ad030f3d1030e8262de1322cd0ccd
)

FetchContent_Declare(
  gameinput
  GIT_REPOSITORY https://github.com/microsoftconnect/GameInput.git
  GIT_TAG v3.3.195.0
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(godot-cpp gameinput)

set(LIB_NAME timestamp_input)

add_library(${LIB_NAME} SHARED)

target_sources(${LIB_NAME}
  PRIVATE
    src/extension_entry.cpp
    src/register_types.cpp
    src/timestamp_input_event.cpp
    src/timestamp_input.cpp
    ${gameinput_SOURCE_DIR}/lib/GameInput.cpp
)

target_include_directories(${LIB_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${gameinput_SOURCE_DIR}/include
)

target_compile_features(${LIB_NAME} PRIVATE cxx_std_17)
target_compile_definitions(${LIB_NAME} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(${LIB_NAME}
  PRIVATE
    godot::cpp
)

set_target_properties(${LIB_NAME} PROPERTIES
  PREFIX ""
  OUTPUT_NAME "timestamp_input.windows.${GODOTCPP_TARGET}.x86_64"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
