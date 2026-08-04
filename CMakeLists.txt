# acestudio-tempo-map-calc-core — the canonical tick<->seconds tempo-map conversion core. This
# CMake drives the NATIVE build: the static core library and its tests, built under stock clang++
# with nothing extra on the include path, so "dependency-free" is enforced by construction. The
# WebAssembly build is produced separately by wasm/build.sh (stock Emscripten) — see
# docs/building.md; it
# compiles the SAME src/TempoCurveCore.cpp.
cmake_minimum_required(VERSION 3.20)
project(acestudio_tempo_map_calc_core CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

enable_testing()

# --- the canonical core (one small TU; no Qt, no dependencies) -----------------------------
add_library(tempo_curve_core STATIC src/TempoCurveCore.cpp)
target_include_directories(tempo_curve_core PUBLIC include)
target_compile_features(tempo_curve_core PUBLIC cxx_std_23)

# --- tst_TempoCurveCore: the isolated analytic unit test (independent oracle) ----------------
add_executable(tst_TempoCurveCore test/tst_TempoCurveCore.cpp)
target_link_libraries(tst_TempoCurveCore PRIVATE tempo_curve_core)
add_test(NAME tst_TempoCurveCore COMMAND tst_TempoCurveCore)

# --- fixture-driven test (only when Python is available) -------------------------------------
# The core library and the analytic unit test above have no dependencies. The shared-fixture test
# needs Python only to turn the fixture JSON into a C++ header at build time (no runtime JSON
# parser), so it is optional — a Python-less environment still builds and tests the core.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    set(FIXTURE_JSON ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tempo_curve_cases.json)
    set(FIXTURE_HDR ${CMAKE_CURRENT_BINARY_DIR}/generated/fixture_generated.h)
    add_custom_command(
        OUTPUT ${FIXTURE_HDR}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/json_to_header.py ${FIXTURE_JSON} ${FIXTURE_HDR}
        DEPENDS ${FIXTURE_JSON} ${CMAKE_CURRENT_SOURCE_DIR}/tools/json_to_header.py
        COMMENT "Generating fixture_generated.h from the shared fixture JSON")
    add_custom_target(fixture_header DEPENDS ${FIXTURE_HDR})

    # run_fixture drives the core over the shared fixture (scalar + bulk range), asserts vs the
    # golden, and emits build/native.json — what wasm/run_fixture.mjs compares the WASM build to.
    add_executable(run_fixture test/run_fixture.cpp)
    add_dependencies(run_fixture fixture_header)
    target_include_directories(run_fixture PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(run_fixture PRIVATE tempo_curve_core)
    add_test(NAME run_fixture COMMAND run_fixture ${CMAKE_CURRENT_BINARY_DIR}/native.json)
else()
    message(STATUS "Python3 not found — skipping the shared-fixture test (core + unit test still built)")
endif()

# --- gen_fixture: dev tool to regenerate the shared fixture from the canonical core ----------
# Off by default — only needed when the oracle set changes. See tools/gen_fixture.cpp.
option(BUILD_FIXTURE_GENERATOR "Build the gen_fixture dev tool" OFF)
if(BUILD_FIXTURE_GENERATOR)
    add_executable(gen_fixture tools/gen_fixture.cpp)
    target_link_libraries(gen_fixture PRIVATE tempo_curve_core)
endif()
