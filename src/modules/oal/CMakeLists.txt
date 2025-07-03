add_library(oal)

target_sources(oal
    PRIVATE
		consumer/oal_consumer.cpp

		oal.cpp

    PUBLIC FILE_SET CXX_MODULES FILES
		consumer/oal_consumer.cppm

		oal.cppm
)

target_include_directories(oal PRIVATE
    ${FFMPEG_INCLUDE_PATH}
)
target_link_libraries(oal
    PRIVATE
        OpenAL::OpenAL
        Boost::rational
        core
        common
)
target_include_directories(oal PRIVATE
        ../..
)


set_target_properties(oal PROPERTIES FOLDER modules)

target_compile_features(oal PRIVATE cxx_std_26)
source_group(sources\\consumer consumer/*)
source_group(sources ./*)
