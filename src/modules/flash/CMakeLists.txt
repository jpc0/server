cmake_minimum_required (VERSION 3.16)
project (flash)

set(SOURCES
		interop/Flash9e_i.c

		producer/FlashAxContainer.cpp
		producer/flash_producer.cpp

		util/swf.cpp

		flash.cpp
)
set(HEADERS
		interop/axflash.h
		interop/TimerHelper.h

		producer/FlashAxContainer.h
		producer/flash_producer.h

		util/swf.h

		flash.h
		StdAfx.h
)

casparcg_add_module_project(flash
	SOURCES ${SOURCES} ${HEADERS}
	INIT_FUNCTION "flash::init"
)
target_include_directories(flash PRIVATE
    ..
    ../..
    ${ZLIB_INCLUDE_PATH}
    )
target_precompile_headers(flash PRIVATE "StdAfx.h")
target_link_libraries(flash zlibstatic.lib)

set_target_properties(flash PROPERTIES FOLDER modules)
source_group(sources\\interop interop/*)
source_group(sources\\producer producer/*)
source_group(sources\\util util/*)
source_group(sources ./*)


