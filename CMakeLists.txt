cmake_minimum_required(VERSION 3.20)

project(Ferrum
    VERSION 0.4.0
    DESCRIPTION "A compiled language for replacing bash scripts that outgrew bash"
    LANGUAGES CXX)

# --- Language standard: strict C++20, no compiler extensions -----------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Lets clangd and other tools find the compile flags (see .gitignore).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config generators: development builds are Debug unless told otherwise.
get_property(FERRUM_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT FERRUM_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Warnings and sanitizers --------------------------------------------------
# Carried by an INTERFACE target so every Ferrum target gets exactly the same
# flags. Warnings are errors; there are no exceptions.
add_library(ferrum_options INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ferrum_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)

    # Debug builds run with AddressSanitizer and UndefinedBehaviorSanitizer.
    target_compile_options(ferrum_options INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)
    target_link_options(ferrum_options INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
else()
    message(WARNING
        "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}': strict warnings and "
        "sanitizers are not configured. Use clang or gcc.")
endif()

# --- ferrum_core: the compiler as a static library ---------------------------
# Everything under src/ except main.cpp. CONFIGURE_DEPENDS re-runs the glob at
# build time, so new files are picked up without re-running cmake by hand.
file(GLOB_RECURSE FERRUM_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER FERRUM_CORE_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")

add_library(ferrum_core STATIC ${FERRUM_CORE_SOURCES})
target_include_directories(ferrum_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(ferrum_core PUBLIC FERRUM_VERSION="${PROJECT_VERSION}")
target_link_libraries(ferrum_core PUBLIC ferrum_options)

# --- ferrumc: the command-line compiler --------------------------------------
add_executable(ferrumc src/main.cpp)
target_link_libraries(ferrumc PRIVATE ferrum_core)

# --- LLVM (Phase 2) -----------------------------------------------------------
# Uncomment when code generation starts. Keep LLVM headers SYSTEM so that
# -Werror does not trip over warnings in code we do not own. If LLVM was built
# without RTTI, also add -fno-rtti to ferrum_options.
#
# find_package(LLVM REQUIRED CONFIG)
# message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")
# separate_arguments(FERRUM_LLVM_DEFS UNIX_COMMAND "${LLVM_DEFINITIONS}")
# target_include_directories(ferrum_core SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
# target_compile_definitions(ferrum_core PUBLIC ${FERRUM_LLVM_DEFS})
# llvm_map_components_to_libnames(FERRUM_LLVM_LIBS core support irreader target
#                                 x86codegen x86asmparser)
# target_link_libraries(ferrum_core PUBLIC ${FERRUM_LLVM_LIBS})

# --- Tests (ctest) ------------------------------------------------------------
enable_testing()

# Smoke test: the binary runs and reports the project version.
add_test(NAME ferrumc_version COMMAND ferrumc --version)
set_tests_properties(ferrumc_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^ferrumc ${PROJECT_VERSION}")

# Every tests/test_*.cpp becomes its own executable (plain main(), exit code 0
# on success, no test framework) linked against ferrum_core and registered
# with ctest under the file's name.
file(GLOB FERRUM_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp")
foreach(test_source IN LISTS FERRUM_TEST_SOURCES)
    get_filename_component(test_name "${test_source}" NAME_WE)
    add_executable(${test_name} "${test_source}")
    target_link_libraries(${test_name} PRIVATE ferrum_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
