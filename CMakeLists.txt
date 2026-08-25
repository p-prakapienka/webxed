cmake_minimum_required(VERSION 3.20)
project(webxed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(webxed
    src/main.cpp
    src/synth/FmVoice.cpp
)

target_include_directories(webxed PRIVATE src)

target_compile_options(webxed PRIVATE -Wall -Wextra -Wpedantic)

if(EMSCRIPTEN)
    set_target_properties(webxed PROPERTIES
        OUTPUT_NAME "webxed"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web/generated"
    )

    target_link_options(webxed PRIVATE
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sENVIRONMENT=web"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_FUNCTIONS=['_createSynth','_destroySynth','_noteOn','_noteOff','_renderSample']"
    )
endif()
