cmake_minimum_required(VERSION 3.26)
project(ScarlettBridge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "ScarlettBridge builds x64 only (pointer size check failed).")
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(SB_ENABLE_QT "Build the Qt Widgets control-center UI (requires Qt6)" ON)
option(SB_WARNINGS_AS_ERRORS "Treat warnings as errors (CI)" OFF)
if(SB_WARNINGS_AS_ERRORS AND MSVC)
  add_compile_options(/WX)
elseif(SB_WARNINGS_AS_ERRORS)
  add_compile_options(-Werror)
endif()

set(SB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---- core: result, json, hashing, logging, config, version (portable, zero deps) ----
add_library(sb_core STATIC
  ${SB_SRC}/core/json.cpp
  ${SB_SRC}/core/hash.cpp
  ${SB_SRC}/core/logging.cpp
  ${SB_SRC}/core/config.cpp
  ${SB_SRC}/core/filesystem_guards.cpp)
target_include_directories(sb_core PUBLIC ${SB_SRC})

# ---- formats: bounded binary reader + PE import census ----
add_library(sb_formats STATIC
  ${SB_SRC}/formats/bounded_reader.cpp
  ${SB_SRC}/formats/pe_imports.cpp
  ${SB_SRC}/formats/pe_exports.cpp)
target_include_directories(sb_formats PUBLIC ${SB_SRC})
target_link_libraries(sb_formats PUBLIC sb_core)

# ---- ipc: versioned protocol + Windows named-pipe transport ----
add_library(sb_ipc STATIC
  ${SB_SRC}/ipc/protocol.cpp
  ${SB_SRC}/ipc/named_pipe.cpp)
target_include_directories(sb_ipc PUBLIC ${SB_SRC})
target_link_libraries(sb_ipc PUBLIC sb_core)

# ---- runtime: lifecycle, synthetic module loader, import registry, services ----
add_library(sb_runtime STATIC
  ${SB_SRC}/runtime/lifecycle.cpp
  ${SB_SRC}/runtime/sbm.cpp
  ${SB_SRC}/runtime/imports.cpp
  ${SB_SRC}/runtime/resolve.cpp
  ${SB_SRC}/runtime/forwarder.cpp
  ${SB_SRC}/runtime/image_layout.cpp
  ${SB_SRC}/runtime/routes.cpp
  ${SB_SRC}/runtime/init_order.cpp
  ${SB_SRC}/runtime/host_path.cpp
  ${SB_SRC}/runtime/console_timer.cpp
  ${SB_SRC}/runtime/stubs.cpp
  ${SB_SRC}/runtime/title.cpp
  ${SB_SRC}/runtime/platform_services.cpp)
target_include_directories(sb_runtime PUBLIC ${SB_SRC})
target_link_libraries(sb_runtime PUBLIC sb_core sb_formats)

# ---- graphics: abstraction, adapter choice, D3D12 backend (Windows), shader IR/translation, cache ----
set(SB_GRAPHICS_SOURCES
  ${SB_SRC}/graphics/adapter_select.cpp
  ${SB_SRC}/graphics/shader_ir.cpp
  ${SB_SRC}/graphics/shader_tally.cpp
  ${SB_SRC}/graphics/shader_translate.cpp
  ${SB_SRC}/graphics/pipeline_cache.cpp)
if(WIN32)
  list(APPEND SB_GRAPHICS_SOURCES ${SB_SRC}/graphics/d3d12_backend.cpp)
endif()
add_library(sb_graphics STATIC ${SB_GRAPHICS_SOURCES})
target_include_directories(sb_graphics PUBLIC ${SB_SRC})
target_link_libraries(sb_graphics PUBLIC sb_core)
if(WIN32)
  target_link_libraries(sb_graphics PRIVATE dxgi d3d12 d3dcompiler)
  target_compile_definitions(sb_graphics PRIVATE SB_HAS_D3D12=1)
endif()

# ---- media: audio/input backends (stubs report Unsupported until implemented) ----
add_library(sb_media STATIC
  ${SB_SRC}/media/backends.cpp
  ${SB_SRC}/media/input_script.cpp)
target_include_directories(sb_media PUBLIC ${SB_SRC})
target_link_libraries(sb_media PUBLIC sb_core)
if(WIN32)
  target_link_libraries(sb_media PUBLIC xinput)  # XInputGetState presence check
endif()

# ---- platform: Windows system info + filesystem guards ----
add_library(sb_platform STATIC ${SB_SRC}/platform/windows_system.cpp)
target_include_directories(sb_platform PUBLIC ${SB_SRC})
target_link_libraries(sb_platform PUBLIC sb_core)

# ---- telemetry: counters, trace ring, redaction ----
add_library(sb_telemetry STATIC
  ${SB_SRC}/telemetry/metrics.cpp
  ${SB_SRC}/telemetry/trace_filter.cpp
  ${SB_SRC}/telemetry/fault_filter.cpp
  ${SB_SRC}/telemetry/engine_stats.cpp
  ${SB_SRC}/telemetry/output_compare.cpp
  ${SB_SRC}/telemetry/frame_box.cpp
  ${SB_SRC}/telemetry/driver_watch.cpp
  ${SB_SRC}/telemetry/refusal_census.cpp)
target_include_directories(sb_telemetry PUBLIC ${SB_SRC})
target_link_libraries(sb_telemetry PUBLIC sb_core)

set(SB_RUNTIME_LIBS sb_runtime sb_ipc sb_core sb_graphics sb_formats sb_media sb_platform sb_telemetry)

# ---- strict CLI option parsing (portable, unit-tested) ----
add_library(sb_cli STATIC
  ${SB_SRC}/app/cli_parse.cpp)
target_include_directories(sb_cli PUBLIC ${SB_SRC})
target_link_libraries(sb_cli PUBLIC sb_core)

# ---- ui support: portable Qt-shell helpers (no Qt, unit-tested) ----
add_library(sb_ui_support STATIC
  ${SB_SRC}/ui/backend_status.cpp)
target_include_directories(sb_ui_support PUBLIC ${SB_SRC})
target_link_libraries(sb_ui_support PUBLIC ${SB_RUNTIME_LIBS})

# ---- executables ----
add_executable(scarlettbridge-runtime ${SB_SRC}/app/runtime_main.cpp)
target_link_libraries(scarlettbridge-runtime PRIVATE ${SB_RUNTIME_LIBS} sb_cli)

add_executable(scarlettbridge-cli ${SB_SRC}/app/cli_main.cpp)
target_link_libraries(scarlettbridge-cli PRIVATE ${SB_RUNTIME_LIBS} sb_cli)

# ---- tools ----
foreach(tool gpu_probe shader_probe fixture_builder cache_inspector)
  add_executable(sb_${tool} ${SB_SRC}/tools/${tool}.cpp)
  target_link_libraries(sb_${tool} PRIVATE ${SB_RUNTIME_LIBS})
endforeach()

# ---- unit + integration tests (CPU-only, portable) ----
add_executable(sb_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp)
target_link_libraries(sb_tests PRIVATE ${SB_RUNTIME_LIBS} sb_cli sb_ui_support)
enable_testing()
add_test(NAME sb_unit COMMAND sb_tests)

# ---- Qt UI (optional; skipped gracefully without Qt6) ----
if(SB_ENABLE_QT)
  find_package(Qt6 QUIET COMPONENTS Widgets)
  if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    add_executable(scarlettbridge-ui
      ${SB_SRC}/app/ui_main.cpp
      ${SB_SRC}/ui/ui_shell.cpp
      ${SB_SRC}/ui/ipc_client.cpp)
    target_include_directories(scarlettbridge-ui PRIVATE ${SB_SRC})
    target_link_libraries(scarlettbridge-ui PRIVATE sb_ui_support ${SB_RUNTIME_LIBS} Qt6::Widgets)
    message(STATUS "Qt6 found: building scarlettbridge-ui")
  else()
    message(STATUS "Qt6 not found: skipping scarlettbridge-ui (core/runtime/tools/tests still build)")
  endif()
endif()
