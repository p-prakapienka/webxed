cmake_minimum_required(VERSION 3.20)
project(webxed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)

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

add_library(webxed_digitone STATIC
    src/model/digitone/DigitoneEnvelope.cpp
    src/model/digitone/DigitonePatch.cpp
    src/serialization/digitone/DigitonePatchSerializer.cpp
)
target_include_directories(webxed_digitone PUBLIC src)
target_link_libraries(webxed_digitone PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(webxed_digitone PRIVATE -Wall -Wextra -Wpedantic)

add_executable(webxed
    src/model/DxPatch.cpp
    src/parser/sysex/DxSysexParser.cpp
    src/synth/dx/DxEngine.cpp
    src/wasm/WebxedSession.cpp
    src/wasm/WasmBridge.cpp
)

target_include_directories(webxed PRIVATE src)
target_link_libraries(webxed PRIVATE dexed_msfa webxed_digitone)
target_compile_options(webxed PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_executable(webxed_tests
        tests/model/digitone/DigitonePatchTest.cpp
    )
    target_link_libraries(webxed_tests PRIVATE webxed_digitone)
    target_compile_options(webxed_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME webxed_tests COMMAND webxed_tests)
endif()

if(EMSCRIPTEN)
    set(WEB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/web")
    file(MAKE_DIRECTORY "${WEB_OUTPUT_DIR}")
    configure_file(src/web/index.html "${WEB_OUTPUT_DIR}/index.html" COPYONLY)
    configure_file(src/web/app.js "${WEB_OUTPUT_DIR}/app.js" COPYONLY)
    configure_file(src/web/AudioEngine.js "${WEB_OUTPUT_DIR}/AudioEngine.js" COPYONLY)
    configure_file(src/web/PatchBrowser.js "${WEB_OUTPUT_DIR}/PatchBrowser.js" COPYONLY)
    configure_file(src/web/SysexLoader.js "${WEB_OUTPUT_DIR}/SysexLoader.js" COPYONLY)
    configure_file(src/web/WebxedApi.js "${WEB_OUTPUT_DIR}/WebxedApi.js" COPYONLY)

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
        "-sEXPORTED_FUNCTIONS=['_malloc','_free','_createSynth','_destroySynth','_loadSysex','_patchCount','_patchName','_selectPatch','_noteOn','_noteOff','_renderSample']"
    )
endif()
