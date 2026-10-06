cmake_minimum_required(VERSION 3.20)
project(evolving_life LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Determinism: never -ffast-math, and no automatic FMA contraction. See DECISIONS.md.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(EVO_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion)
  set(EVO_FP_FLAGS -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  set(EVO_WARNINGS /W4)
  set(EVO_FP_FLAGS /fp:precise)
endif()

add_library(evo_core STATIC
  core/rng.cpp
  core/environment.cpp
  core/mutation.cpp
  core/world.cpp
  core/phases.cpp
)
target_include_directories(evo_core PUBLIC core)
target_compile_options(evo_core PRIVATE ${EVO_WARNINGS} PUBLIC ${EVO_FP_FLAGS})

add_executable(evolve cli/main.cpp)
target_link_libraries(evolve PRIVATE evo_core)
target_compile_options(evolve PRIVATE ${EVO_WARNINGS})

enable_testing()
add_executable(evo_tests
  tests/test_main.cpp
  tests/test_rng.cpp
  tests/test_environment.cpp
  tests/test_invariants.cpp
  tests/test_mutation.cpp
  tests/test_cells.cpp
)
target_include_directories(evo_tests PRIVATE tests)
target_link_libraries(evo_tests PRIVATE evo_core)
add_test(NAME evo_tests COMMAND evo_tests)
