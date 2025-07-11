cmake_minimum_required (VERSION 3.16)
project (accelerator)

set(SOURCES
	ogl/image/image_kernel.cpp
	ogl/image/image_mixer.cpp
	ogl/image/image_shader.cpp

	ogl/util/buffer.cpp
	ogl/util/device.cpp
	ogl/util/shader.cpp
	ogl/util/texture.cpp
	ogl/util/matrix.cpp
	ogl/util/transforms.cpp

	accelerator.cpp
)
set(HEADERS
	ogl/image/image_kernel.h
	ogl/image/image_mixer.h
	ogl/image/image_shader.h

	ogl/util/buffer.h
	ogl/util/context.h
	ogl/util/device.h
	ogl/util/shader.h
	ogl/util/texture.h
	ogl/util/matrix.h
	ogl/util/transforms.h

	ogl_image_vertex.h
	ogl_image_fragment.h

	accelerator.h
	StdAfx.h
)

if (MSVC)
	list(APPEND SOURCES
		ogl/util/context_sfml.cpp
	)
else()
	list(APPEND SOURCES
		ogl/util/context_egl.cpp
	)
endif()

bin2c("ogl/image/shader.vert" "ogl_image_vertex.h" "caspar::accelerator::ogl" "vertex_shader")
bin2c("ogl/image/shader.frag" "ogl_image_fragment.h" "caspar::accelerator::ogl" "fragment_shader")

casparcg_add_library(accelerator SOURCES ${SOURCES} ${HEADERS})
target_include_directories(accelerator PRIVATE .. ${CMAKE_CURRENT_BINARY_DIR})
target_precompile_headers(accelerator PRIVATE StdAfx.h)
target_link_libraries(accelerator
    PRIVATE
        common
        core
        GLEW::glew
        Boost::asio
        Boost::rational
        Boost::numeric_ublas
)

if (MSVC)
target_link_libraries(accelerator PRIVATE SFML::Window)
endif()

source_group(sources ./.*)
source_group(sources\\cpu\\image cpu/image/.*)
source_group(sources\\cpu\\util cpu/util/.*)
source_group(sources\\ogl\\image ogl/image/.*)
source_group(sources\\ogl\\util ogl/util/.*)
