cmake_minimum_required (VERSION 3.16)
project (ffmpeg)

set(SOURCES
	producer/av_producer.cpp
	producer/av_producer.h
	producer/av_input.cpp
	producer/av_input.h
	producer/ffmpeg_producer.cpp
	producer/ffmpeg_producer.h
	consumer/ffmpeg_consumer.cpp
	consumer/ffmpeg_consumer.h

	util/av_util.cpp
	util/av_util.h
	util/av_assert.h

	ffmpeg.cpp
	ffmpeg.h
	defines.h
	StdAfx.h
)

casparcg_add_module_project(ffmpeg
	SOURCES ${SOURCES}
	INIT_FUNCTION "ffmpeg::init"
	UNINIT_FUNCTION "ffmpeg::uninit"
)

target_include_directories(ffmpeg PRIVATE
    ${FFMPEG_INCLUDE_PATH}
    )
target_precompile_headers(ffmpeg PRIVATE "StdAfx.h")

set_target_properties(ffmpeg PROPERTIES FOLDER modules)
source_group(sources ./*)
source_group(sources\\consumer ./consumer/.*)
source_group(sources\\producer ./producer/.*)
source_group(sources\\util ./util/.*)
target_link_libraries(ffmpeg
    PRIVATE
        Boost::algorithm
        Boost::filesystem
        Boost::format
        Boost::lexical_cast
        Boost::rational
        Boost::property_tree
        Boost::log
        Boost::signals2
        Boost::logic
        Boost::crc
)

if (MSVC)
	# target_link_libraries(ffmpeg
	# )
else()
	target_link_libraries(ffmpeg PRIVATE
			${FFMPEG_LIBRARIES}
	)
endif()
