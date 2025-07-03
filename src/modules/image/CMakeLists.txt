add_library(image)
target_sources(image
    PRIVATE
		consumer/image_consumer.cpp

		producer/image_producer.cpp

		producer/image_scroll_producer.cpp

		util/image_algorithms.cpp
		util/image_converter.cpp
		util/image_loader.cpp

		image.cpp

    PUBLIC FILE_SET HEADERS FILES


    PUBLIC FILE_SET CXX_MODULES FILES
		consumer/image_consumer.cppm

		producer/image_producer.cppm

		producer/image_scroll_producer.cppm

        util/image_algorithms.cppm
		util/image_converter.cppm
		util/image_loader.cppm
		util/image_view.cppm
		util/image_util.cppm

		image.cppm
)
target_include_directories(image PRIVATE
	..
	${FFMPEG_INCLUDE_PATH}
    ../..
)

target_compile_features(image PRIVATE cxx_std_26)

set_target_properties(image PROPERTIES FOLDER modules)
source_group(sources\\consumer consumer/*)
source_group(sources\\producer producer/*)
source_group(sources\\util util/*)
source_group(sources ./*)

target_link_libraries(image PRIVATE
    core
    common

	ffmpeg
    Boost::lexical_cast
    Boost::variant
    Boost::rational
)

