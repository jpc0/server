cmake_minimum_required (VERSION 3.16)

set(SOURCES
	consumer/artnet_consumer.cpp
	consumer/artnet_consumer.h

	util/fixture_calculation.cpp
	util/fixture_calculation.h

	artnet.cpp
	artnet.h
)

casparcg_add_module_project(artnet
	SOURCES ${SOURCES}
	INIT_FUNCTION "artnet::init"
)

set_target_properties(artnet PROPERTIES FOLDER modules)
source_group(sources\\consumer consumer/*)
source_group(sources\\util util/*)
source_group(sources ./*)
target_link_libraries(artnet
    PRIVATE
        Boost::rational
        Boost::asio
)
