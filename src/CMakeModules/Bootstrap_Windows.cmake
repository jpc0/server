cmake_minimum_required (VERSION 3.16)

include(ExternalProject)
include(FetchContent)

if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()
# Prefer the new boost helper
if(POLICY CMP0167)
	cmake_policy(SET CMP0167 NEW)
endif()

set(BOOST_USE_PRECOMPILED ON CACHE BOOL "Use precompiled boost")

set(CASPARCG_RUNTIME_DEPENDENCIES_RELEASE "" CACHE INTERNAL "")
set(CASPARCG_RUNTIME_DEPENDENCIES_DEBUG "" CACHE INTERNAL "")
set(CASPARCG_RUNTIME_DEPENDENCIES_RELEASE_DIRS "" CACHE INTERNAL "")
set(CASPARCG_RUNTIME_DEPENDENCIES_DEBUG_DIRS "" CACHE INTERNAL "")

function(casparcg_add_runtime_dependency FILE_TO_COPY)
	if ("${ARGV1}" STREQUAL "Release" OR NOT ARGV1)
		set(CASPARCG_RUNTIME_DEPENDENCIES_RELEASE "${CASPARCG_RUNTIME_DEPENDENCIES_RELEASE}" "${FILE_TO_COPY}" CACHE INTERNAL "")
	endif()
	if ("${ARGV1}" STREQUAL "Debug" OR NOT ARGV1)
		set(CASPARCG_RUNTIME_DEPENDENCIES_DEBUG "${CASPARCG_RUNTIME_DEPENDENCIES_DEBUG}" "${FILE_TO_COPY}" CACHE INTERNAL "")
	endif()
endfunction()
function(casparcg_add_runtime_dependency_dir FILE_TO_COPY)
	if ("${ARGV1}" STREQUAL "Release" OR NOT ARGV1)
		set(CASPARCG_RUNTIME_DEPENDENCIES_RELEASE_DIRS "${CASPARCG_RUNTIME_DEPENDENCIES_RELEASE_DIRS}" "${FILE_TO_COPY}" CACHE INTERNAL "")
	endif()
	if ("${ARGV1}" STREQUAL "Debug" OR NOT ARGV1)
		set(CASPARCG_RUNTIME_DEPENDENCIES_DEBUG_DIRS "${CASPARCG_RUNTIME_DEPENDENCIES_DEBUG_DIRS}" "${FILE_TO_COPY}" CACHE INTERNAL "")
	endif()
endfunction()
function(casparcg_add_runtime_dependency_from_target TARGET)
	get_target_property(_runtime_lib_name ${TARGET} IMPORTED_LOCATION_RELEASE)
	if (NOT "${_runtime_lib_name}" STREQUAL "")
		set(CASPARCG_RUNTIME_DEPENDENCIES_RELEASE "${CASPARCG_RUNTIME_DEPENDENCIES_RELEASE}" "${_runtime_lib_name}" CACHE INTERNAL "")
	endif()

	get_target_property(_runtime_lib_name ${TARGET} IMPORTED_LOCATION_DEBUG)
	if (NOT "${_runtime_lib_name}" STREQUAL "")
		set(CASPARCG_RUNTIME_DEPENDENCIES_DEBUG "${CASPARCG_RUNTIME_DEPENDENCIES_DEBUG}" "${_runtime_lib_name}" CACHE INTERNAL "")
	endif()
endfunction()

casparcg_add_runtime_dependency("${PROJECT_SOURCE_DIR}/shell/casparcg.config")

# BOOST
casparcg_add_external_project(boost)
if (BOOST_USE_PRECOMPILED)
	ExternalProject_Add(boost
	URL ${CASPARCG_DOWNLOAD_MIRROR}/boost/boost_1_74_0-win32-x64-debug-release.zip
	URL_HASH MD5=8d379b0da9a5ae50a3980d2fc1a24d34
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	)
	ExternalProject_Get_Property(boost SOURCE_DIR)
	set(BOOST_INCLUDE_PATH "${SOURCE_DIR}/include/boost-1_74")
	link_directories("${SOURCE_DIR}/lib")
else ()
	set(BOOST_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/boost-install)
	ExternalProject_Add(boost
	URL ${CASPARCG_DOWNLOAD_MIRROR}/boost/boost_1_74_0.zip
	URL_HASH MD5=df1456965493f05952b7c06205688ae9
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ./bootstrap.bat
		--with-libraries=filesystem
		--with-libraries=locale
		--with-libraries=log
		--with-libraries=log_setup
		--with-libraries=regex
		--with-libraries=system
		--with-libraries=thread
	BUILD_COMMAND ./b2 install debug release --prefix=${BOOST_INSTALL_DIR} link=static threading=multi runtime-link=shared -j ${CONFIG_CPU_COUNT}
	INSTALL_COMMAND ""
	)
	set(BOOST_INCLUDE_PATH "${BOOST_INSTALL_DIR}/include/boost-1_74")
	link_directories("${BOOST_INSTALL_DIR}/lib")
endif ()
add_definitions( -DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE )
add_definitions( -DBOOST_COROUTINES_NO_DEPRECATION_WARNING )
add_definitions( -DBOOST_LOCALE_HIDE_AUTO_PTR )

# FFMPEG
casparcg_add_external_project(ffmpeg-lib)
ExternalProject_Add(ffmpeg-lib
	URL ${CASPARCG_DOWNLOAD_MIRROR}/ffmpeg/ffmpeg-7.0.2-full_build-shared.7z
	URL_HASH MD5=c5127aeed36a9a86dd3b84346be182f8
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
)
ExternalProject_Get_Property(ffmpeg-lib SOURCE_DIR)
set(FFMPEG_INCLUDE_PATH "${SOURCE_DIR}/include")
set(FFMPEG_BIN_PATH "${SOURCE_DIR}/bin")
link_directories("${SOURCE_DIR}/lib")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/avcodec-61.dll")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/avdevice-61.dll")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/avfilter-10.dll")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/avformat-61.dll")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/avutil-59.dll")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/postproc-58.dll")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/swresample-5.dll")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/swscale-8.dll")
# for scanner:
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/ffmpeg.exe")
casparcg_add_runtime_dependency("${FFMPEG_BIN_PATH}/ffprobe.exe")

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

set(EXTERNAL_CMAKE_ARGS "")
if (is_multi_config)
	set(EXTERNAL_CMAKE_ARGS "-DCMAKE_BUILD_TYPE:STRING=$<CONFIG>")
else()
	set(EXTERNAL_CMAKE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}")
endif ()

# TBB
FetchContent_Declare(tbb
	URL ${CASPARCG_DOWNLOAD_MIRROR}/tbb/oneapi-tbb-2021.1.1-win.zip
	URL_HASH MD5=51bf49044d477dea67670abd92f8814c
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
)
FetchContent_MakeAvailable(tbb)

list(APPEND CMAKE_PREFIX_PATH ${tbb_SOURCE_DIR}/lib/cmake/tbb)
find_package(tbb REQUIRED)

casparcg_add_runtime_dependency_from_target(TBB::tbb)
casparcg_add_runtime_dependency_from_target(TBB::tbbmalloc)
casparcg_add_runtime_dependency_from_target(TBB::tbbmalloc_proxy)

# GLEW
FetchContent_Declare(glew
	URL ${CASPARCG_DOWNLOAD_MIRROR}/glew/glew-2.2.0-win32.zip
	URL_HASH MD5=1feddfe8696c192fa46a0df8eac7d4bf
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
)
FetchContent_MakeAvailable(glew)

add_library(GLEW::glew INTERFACE IMPORTED)
target_include_directories(GLEW::glew INTERFACE ${glew_SOURCE_DIR}/include)
target_link_directories(GLEW::glew INTERFACE ${glew_SOURCE_DIR}/lib/Release/x64)
target_link_libraries(GLEW::glew INTERFACE glew32)
casparcg_add_runtime_dependency("${glew_SOURCE_DIR}/bin/Release/x64/glew32.dll")

# SFML
FetchContent_Declare(sfml
	URL ${CASPARCG_DOWNLOAD_MIRROR}/sfml/SFML-2.6.2-windows-vc17-64-bit.zip
	URL_HASH MD5=dee0602d6f94d1843eef4d7568d2c23d
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
)
FetchContent_MakeAvailable(sfml)

list(APPEND CMAKE_PREFIX_PATH ${sfml_SOURCE_DIR}/lib/cmake/SFML)
# set(SFML_STATIC_LIBRARIES TRUE)
find_package(SFML 2 COMPONENTS graphics window REQUIRED)

casparcg_add_runtime_dependency_from_target(sfml-graphics)
casparcg_add_runtime_dependency_from_target(sfml-window)
casparcg_add_runtime_dependency_from_target(sfml-system)

#ZLIB
casparcg_add_external_project(zlib)
ExternalProject_Add(zlib
	URL ${CASPARCG_DOWNLOAD_MIRROR}/zlib/zlib-1.3.tar.gz
	URL_HASH MD5=60373b133d630f74f4a1f94c1185a53f
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
	CMAKE_ARGS ${EXTERNAL_CMAKE_ARGS}
	INSTALL_COMMAND ""
)
ExternalProject_Get_Property(zlib SOURCE_DIR)
ExternalProject_Get_Property(zlib BINARY_DIR)
set(ZLIB_INCLUDE_PATH "${SOURCE_DIR};${BINARY_DIR}")

if (is_multi_config)
	link_directories(${BINARY_DIR}/Release)
else()
	link_directories(${BINARY_DIR})
endif()

# OpenAL
FetchContent_Declare(openal
	URL ${CASPARCG_DOWNLOAD_MIRROR}/openal/openal-soft-1.19.1-bin.zip
	URL_HASH MD5=b78ef1ba26f7108e763f92df6bbc3fa5
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
)
FetchContent_MakeAvailable(openal)
file(COPY_FILE ${openal_SOURCE_DIR}/bin/Win64/soft_oal.dll ${openal_SOURCE_DIR}/bin/Win64/OpenAL32.dll)

add_library(OpenAL::OpenAL INTERFACE IMPORTED)
target_include_directories(OpenAL::OpenAL INTERFACE ${openal_SOURCE_DIR}/include)
target_link_directories(OpenAL::OpenAL INTERFACE ${openal_SOURCE_DIR}/libs/Win64)
target_link_libraries(OpenAL::OpenAL INTERFACE OpenAL32)
casparcg_add_runtime_dependency("${openal_SOURCE_DIR}/bin/Win64/OpenAL32.dll")

# flash template host
casparcg_add_external_project(flashtemplatehost)
ExternalProject_Add(flashtemplatehost
	URL ${CASPARCG_DOWNLOAD_MIRROR}/flash-template-host/flash-template-host-files.zip
	URL_HASH MD5=360184ce21e34d585d1d898fdd7a6bd8
	DOWNLOAD_DIR ${CASPARCG_DOWNLOAD_CACHE}
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
)
ExternalProject_Get_Property(flashtemplatehost SOURCE_DIR)
set(TEMPLATE_HOST_PATH "${SOURCE_DIR}")
# casparcg_add_runtime_dependency_dir("${TEMPLATE_HOST_PATH}")

# LIBERATION_FONTS
set(LIBERATION_FONTS_BIN_PATH "${PROJECT_SOURCE_DIR}/shell/liberation-fonts")
casparcg_add_runtime_dependency("${LIBERATION_FONTS_BIN_PATH}/LiberationMono-Regular.ttf")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT casparcg)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
add_definitions(-DCASPAR_SOURCE_PREFIX="${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions(-D_WIN32_WINNT=0x601)

# ignore boost deprecated headers, as these are often reported inside boost
add_definitions("-DBOOST_ALLOW_DEPRECATED_HEADERS")

# Ensure /EHsc is not defined as it clashes with EHa below
string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa /Zi /W4 /WX /MP /fp:fast /Zm192 /FIcommon/compiler/vs/disable_silly_warnings.h")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}	/D TBB_USE_ASSERT=1 /D TBB_USE_DEBUG /bigobj")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}	/Oi /Ot /Gy /bigobj")
