cmake_minimum_required(VERSION 3.20)
project(webxed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(DEXED_COMMIT "2e182b3db85c09083ab13c8b9b00565ce7d9ff85")
FetchContent_Declare(
    dexed
    GIT_REPOSITORY https://github.com/asb2m10/dexed.git
    GIT_TAG ${DEXED_COMMIT}
    GIT_SHALLOW FALSE
    GIT_SUBMODULES ""
)
FetchContent_GetProperties(dexed)
if(NOT dexed_POPULATED)
    FetchContent_Populate(dexed)
endif()

add_library(dexed_msfa STATIC
    ${dexed_SOURCE_DIR}/Source/msfa/env.cc
    ${dexed_SOURCE_DIR}/Source/msfa/exp2.cc
    ${dexed_SOURCE_DIR}/Source/msfa/fm_core.cc
    ${dexed_SOURCE_DIR}/Source/msfa/fm_op_kernel.cc
    ${dexed_SOURCE_DIR}/Source/msfa/sin.cc
)

target_include_directories(dexed_msfa PUBLIC
    ${dexed_SOURCE_DIR}/Source
    ${dexed_SOURCE_DIR}/Source/msfa
)

add_executable(webxed
    src/model/DxPatch.cpp
    src/synth/dx/DxEngine.cpp
    src/wasm/WasmBridge.cpp
)

target_include_directories(webxed PRIVATE src)
target_link_libraries(webxed PRIVATE dexed_msfa)
target_compile_options(webxed PRIVATE -Wall -Wextra -Wpedantic)

if(EMSCRIPTEN)
    set(WEB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/web")
    file(MAKE_DIRECTORY "${WEB_OUTPUT_DIR}")
    configure_file(src/web/index.html "${WEB_OUTPUT_DIR}/index.html" COPYONLY)
    configure_file(src/web/app.js "${WEB_OUTPUT_DIR}/app.js" COPYONLY)

    set_target_properties(webxed PROPERTIES
        OUTPUT_NAME "webxed"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${WEB_OUTPUT_DIR}"
    )

    target_link_options(webxed PRIVATE
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sENVIRONMENT=web"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sNO_EXIT_RUNTIME=1"
        "-sEXPORTED_RUNTIME_METHODS=['cwrap']"
        "-sEXPORTED_FUNCTIONS=['_createSynth','_destroySynth','_noteOn','_noteOff','_renderSample']"
    )
endif()
