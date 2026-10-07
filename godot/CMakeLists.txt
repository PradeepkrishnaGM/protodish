# GDExtension wrapper around the core (M7). Built with -DEVO_BUILD_GODOT=ON.
# godot-cpp is pinned at 10.0.0-stable and targets the Godot 4.7 API (DECISIONS M7).
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API version targeted by godot-cpp")
set(GODOTCPP_TARGET "template_debug" CACHE STRING "godot-cpp target")
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "Treat godot-cpp headers as system headers (no warnings from them)")
# Local builds link the system libstdc++ (Fedora ships no static one). Release builds (M8)
# turn this on for portability.
set(GODOTCPP_USE_STATIC_CPP OFF CACHE BOOL "Link libgcc and libstdc++ statically")
add_subdirectory(${PROJECT_SOURCE_DIR}/extern/godot-cpp ${CMAKE_BINARY_DIR}/godot-cpp EXCLUDE_FROM_ALL)

add_library(protodish SHARED
  src/register_types.cpp
  src/protodish_world.cpp
)
target_link_libraries(protodish PRIVATE evo_core godot-cpp)
target_compile_options(protodish PRIVATE ${EVO_WARNINGS})
get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)
set_target_properties(protodish PROPERTIES
  CXX_STANDARD 20
  CXX_EXTENSIONS OFF
  CXX_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
  PREFIX "lib"
  OUTPUT_NAME "protodish${GODOTCPP_SUFFIX}"
)
add_custom_command(TARGET protodish POST_BUILD
  COMMAND ${CMAKE_COMMAND} -DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
          -P ${PROJECT_SOURCE_DIR}/cmake/write_extension_list.cmake)

# Headless tests: import the project (registers the extension), then run the test script.
add_test(NAME godot_import
  COMMAND godot --headless --path ${CMAKE_CURRENT_SOURCE_DIR} --import)
add_test(NAME godot_headless
  COMMAND godot --headless --path ${CMAKE_CURRENT_SOURCE_DIR} -s res://tests/run_tests.gd
          -- --evolve $<TARGET_FILE:evolve> --out ${CMAKE_BINARY_DIR}/godot_out)
set_tests_properties(godot_import PROPERTIES LABELS godot FIXTURES_SETUP godot_project)
set_tests_properties(godot_headless PROPERTIES LABELS godot FIXTURES_REQUIRED godot_project
                     FAIL_REGULAR_EXPRESSION "FAIL|SCRIPT ERROR|ERROR:")
