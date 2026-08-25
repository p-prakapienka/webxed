cmake_minimum_required(VERSION 3.20)
project(webxed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(webxed
    src/wasm/WasmBridge.cpp
    src/synth/FmVoice.cpp
)

target_include_directories(webxed PRIVATE src)
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
