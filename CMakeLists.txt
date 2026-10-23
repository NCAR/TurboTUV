cmake_minimum_required(VERSION 3.20)
project(TurboTUV VERSION 0.1.0 LANGUAGES C)
include(CTest)
include(GNUInstallDirs)

option(TURBOTUV_SANITIZE "Enable address and undefined-behavior sanitizers" OFF)
option(TURBOTUV_BUILD_CLI "Build the standalone POSIX runner" ON)
option(TURBOTUV_NETCDF_OUTPUT "Enable NetCDF output in the standalone runner" OFF)
option(TURBOTUV_BUILD_BENCHMARK "Build the column benchmark (not installed)" ON)
option(TURBOTUV_PROFILE "Benchmark-only stage timers (do not use for acceptance timings)" OFF)
set(TURBOTUV_REAPER_RATE_TILE "" CACHE STRING "Experimental REAPER level tile (0 means full levels)")
option(TURBOTUV_REAPER_OVERLAP "REAPER fixed conserving-interpolation schedules" ON)
option(TURBOTUV_REAPER_OXYGEN "REAPER combined O2 evaluation and reusable workspace" ON)
option(TURBOTUV_REAPER_FLUX_CHECK "REAPER fused flux finiteness check" ON)
option(TURBOTUV_REAPER_SPECTRUM_EXP "REAPER bounded spectrum exponentials (libm fallback)" ON)
option(TURBOTUV_REAPER_MATH_HINTS "Optional guarded compiler hints for independent spectral lanes" ON)
option(TURBOTUV_REAPER_FIXED_CACHE "REAPER exact fixed temperature-range cache" ON)
set(TURBOTUV_REAPER_DIRECT_CACHE "1" CACHE STRING "REAPER clamp cache: 0 off, 1 expensive formulas, 2 all")
set(TURBOTUV_REAPER_BLOCK "16" CACHE STRING "REAPER wavelengths per transfer block")
set(TURBOTUV_REAPER_SOURCES "" CACHE STRING "Optional generated C sources and static registry")
set(TURBOTUV_REFERENCE_BUILD "" CACHE PATH "Configured and built TUV-x reference directory")
set(TURBOTUV_REFERENCE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../TUV-x" CACHE PATH "TUV-x reference checkout")
set(TURBOTUV_REFERENCE_PREFIX "" CACHE PATH "Compiler-matched NetCDF Fortran installation")
set(TURBOTUV_NETCDF_PREFIX "" CACHE PATH "NetCDF C installation (reference tests only)")

add_library(turbotuv src/numerics.c src/geometry.c src/data.c src/radiation.c src/spectra.c
    src/yields.c src/weights.c src/temperature.c src/rates.c src/oxygen.c src/linalg.c
    src/discrete_eigen.c src/discrete_quadrature.c src/discrete.c
    src/grid.c src/profiles.c src/tables.c src/solar_flux.c src/optics.c
    src/config.c src/profile_config.c src/build.c src/spectrum.c src/spectrum_temperature.c
    src/model_build.c src/model_run.c src/model_host.c src/decimal.c
    src/spectrum_plan.c src/model_plan.c src/radiation_block.c src/diagnostics.c
    src/reaper_writer.c src/reaper_descriptor.c src/radiation_reaper.c)
if(TURBOTUV_REAPER_SOURCES)
    target_sources(turbotuv PRIVATE ${TURBOTUV_REAPER_SOURCES})
    target_compile_definitions(turbotuv PRIVATE TT_REAPER_BUILTINS)
else()
    target_sources(turbotuv PRIVATE src/reaper_dispatch.c)
endif()
add_library(TurboTUV::turbotuv ALIAS turbotuv)
target_compile_features(turbotuv PUBLIC c_std_11)
if(TURBOTUV_PROFILE)
    target_compile_definitions(turbotuv PRIVATE TT_PROFILE)
endif()
if(NOT TURBOTUV_REAPER_RATE_TILE STREQUAL "")
    if(NOT TURBOTUV_REAPER_RATE_TILE MATCHES "^(0|1|2|4|8|16|32)$")
        message(FATAL_ERROR "REAPER rate tile must be 0, 1, 2, 4, 8, 16 or 32")
    endif()
    target_compile_definitions(turbotuv PRIVATE TT_REAPER_RATE_TILE=${TURBOTUV_REAPER_RATE_TILE})
endif()
target_compile_definitions(turbotuv PRIVATE
    TT_REAPER_OVERLAP=$<BOOL:${TURBOTUV_REAPER_OVERLAP}>
    TT_REAPER_OXYGEN=$<BOOL:${TURBOTUV_REAPER_OXYGEN}>
    TT_REAPER_FLUX_CHECK=$<BOOL:${TURBOTUV_REAPER_FLUX_CHECK}>
    TT_REAPER_SPECTRUM_EXP=$<BOOL:${TURBOTUV_REAPER_SPECTRUM_EXP}>
    TT_REAPER_MATH_HINTS=$<BOOL:${TURBOTUV_REAPER_MATH_HINTS}>
    TT_REAPER_FIXED_CACHE=$<BOOL:${TURBOTUV_REAPER_FIXED_CACHE}>
    TT_REAPER_DIRECT_CACHE=${TURBOTUV_REAPER_DIRECT_CACHE})
if(NOT TURBOTUV_REAPER_BLOCK MATCHES "^(2|4|8|16|32)$")
    message(FATAL_ERROR "REAPER transfer block must be 2, 4, 8, 16 or 32")
endif()
target_compile_definitions(turbotuv PRIVATE TT_REAPER_BLOCK=${TURBOTUV_REAPER_BLOCK})
if(NOT TURBOTUV_REAPER_DIRECT_CACHE MATCHES "^[012]$")
    message(FATAL_ERROR "REAPER direct cache must be 0, 1 or 2")
endif()
set_target_properties(turbotuv PROPERTIES C_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_include_directories(turbotuv PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(turbotuv PRIVATE third_party/libyaml third_party/libyaml/include)
add_library(turbotuv_yaml OBJECT
    third_party/libyaml/src/api.c third_party/libyaml/src/loader.c
    third_party/libyaml/src/parser.c third_party/libyaml/src/reader.c
    third_party/libyaml/src/scanner.c)
set_target_properties(turbotuv_yaml PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
target_include_directories(turbotuv_yaml PRIVATE third_party/libyaml/include)
target_compile_definitions(turbotuv_yaml PRIVATE YAML_DECLARE_STATIC
    YAML_VERSION_MAJOR=0 YAML_VERSION_MINOR=2 YAML_VERSION_PATCH=5 YAML_VERSION_STRING="0.2.5")
if(MSVC)
    target_compile_options(turbotuv_yaml PRIVATE /FI${CMAKE_CURRENT_SOURCE_DIR}/third_party/libyaml/namespace.h)
else()
    target_compile_options(turbotuv_yaml PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libyaml/namespace.h)
endif()
target_sources(turbotuv PRIVATE $<TARGET_OBJECTS:turbotuv_yaml>)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(turbotuv PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()
if(UNIX)
    target_link_libraries(turbotuv PUBLIC m)
endif()
if(TURBOTUV_SANITIZE)
    target_compile_options(turbotuv_yaml PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(turbotuv PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(turbotuv PUBLIC -fsanitize=address,undefined)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(TURBOTUV_BUILD_CLI)
    add_executable(turbotuv_cli app/main.c app/output.c)
    set_target_properties(turbotuv_cli PROPERTIES OUTPUT_NAME turbotuv C_EXTENSIONS OFF)
    target_link_libraries(turbotuv_cli PRIVATE turbotuv)
    target_compile_definitions(turbotuv_cli PRIVATE TT_VERSION="${PROJECT_VERSION}")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(turbotuv_cli PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
    if(TURBOTUV_NETCDF_OUTPUT)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(NETCDF REQUIRED IMPORTED_TARGET netcdf)
        target_link_libraries(turbotuv_cli PRIVATE PkgConfig::NETCDF)
        target_compile_definitions(turbotuv_cli PRIVATE TT_NETCDF_OUTPUT)
    endif()
    install(TARGETS turbotuv_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TURBOTUV_BUILD_BENCHMARK)
    add_executable(turbotuv_reaper_export tools/reaper/export.c)
    set_target_properties(turbotuv_reaper_export PROPERTIES C_EXTENSIONS OFF)
    target_link_libraries(turbotuv_reaper_export PRIVATE turbotuv)
    add_executable(turbotuv_bench bench/turbotuv_bench.c)
    set_target_properties(turbotuv_bench PROPERTIES C_EXTENSIONS OFF)
    target_link_libraries(turbotuv_bench PRIVATE turbotuv)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(turbotuv_bench PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endif()
install(TARGETS turbotuv EXPORT TurboTUVTargets)
install(FILES include/turbotuv.h include/turbotuv_config.h include/turbotuv_model.h
    include/turbotuv_diagnostics.h include/turbotuv_reaper.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES third_party/libyaml/License DESTINATION ${CMAKE_INSTALL_DATADIR}/TurboTUV/licenses RENAME LibYAML.txt)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/TurboTUV/licenses RENAME TurboTUV.txt)
install(EXPORT TurboTUVTargets NAMESPACE TurboTUV:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TurboTUV)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/TurboTUVConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/TurboTUVConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TurboTUV)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/TurboTUVConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/TurboTUVConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TurboTUVConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TurboTUV)
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/TurboTUV/examples
    FILES_MATCHING PATTERN "*.json" PATTERN "*.yml" PATTERN "*.c")
