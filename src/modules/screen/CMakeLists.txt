add_library(screen)
target_sources(screen
    PRIVATE
        consumer/screen_consumer.cpp
        $<IF:$<STREQUAL:$<PLATFORM_ID>,Linux>,util/x11_util.cpp,"">

        screen.cpp
    PUBLIC FILE_SET CXX_MODULES FILES
        consumer/screen_consumer.cppm
        $<IF:$<STREQUAL:$<PLATFORM_ID>,Linux>,util/x11_util.cppm,"">

        screen.cppm
)

target_compile_features(screen PRIVATE cxx_std_26)

target_compile_options(screen PRIVATE
    "$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wno-c23-extensions>"
)

target_include_directories(screen PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${FFMPEG_INCLUDE_PATH}
    ../..
)
target_link_libraries(screen
    PRIVATE
        common
        core

        accelerator
        ffmpeg
        GLEW::glew
        SFML::Window
        Boost::rational
)

set_target_properties(screen PROPERTIES FOLDER modules)
source_group(sources\\consumer consumer/*)
source_group(sources ./*)
