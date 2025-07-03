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

set(USE_STATIC_BOOST OFF CACHE BOOL "Use shared library version of Boost")
set(CASPARCG_BINARY_NAME "casparcg" CACHE STRING "Custom name of the binary to build (this disables some install files)")

# Determine build (target) platform
SET (PLATFORM_FOLDER_NAME "linux")

IF (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	MESSAGE (STATUS "Setting build type to 'Release' as none was specified.")
	SET (CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
	SET_PROPERTY (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
ENDIF ()
MARK_AS_ADVANCED (CMAKE_INSTALL_PREFIX)

find_package(FFmpeg REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS OpenGL GLX EGL)
find_package(GLEW REQUIRED)
find_package(TBB REQUIRED)
find_package(OpenAL REQUIRED)
find_package(SFML 3 COMPONENTS Graphics Window REQUIRED)
find_package(X11 REQUIRED)

# support for Ubuntu 22.04
if (NOT TARGET OpenAL::OpenAL)
    add_library(OpenAL::OpenAL INTERFACE IMPORTED)
    target_include_directories(OpenAL::OpenAL INTERFACE ${OPENAL_INCLUDE_DIR})
    target_link_libraries(OpenAL::OpenAL INTERFACE ${OPENAL_LIBRARY})
endif()

SET (FFMPEG_INCLUDE_PATH "${FFMPEG_INCLUDE_DIRS}")

LINK_DIRECTORIES("${FFMPEG_LIBRARY_DIRS}")

SET_PROPERTY (GLOBAL PROPERTY USE_FOLDERS ON)

ADD_DEFINITIONS (-DSFML_STATIC)
ADD_DEFINITIONS (-DUNICODE)
ADD_DEFINITIONS (-D_UNICODE)
ADD_DEFINITIONS (-DGLEW_NO_GLU)
ADD_DEFINITIONS (-DGLEW_EGL)
ADD_DEFINITIONS (-D__NO_INLINE__) # Needed for precompiled headers to work
ADD_DEFINITIONS (-DBOOST_NO_SWPRINTF) # swprintf on Linux seems to always use , as decimal point regardless of C-locale or C++-locale
ADD_DEFINITIONS (-DTBB_USE_CAPTURED_EXCEPTION=1)
ADD_DEFINITIONS (-DNDEBUG) # Needed for precompiled headers to work
ADD_DEFINITIONS (-DBOOST_LOCALE_HIDE_AUTO_PTR) # Needed for C++17 in boost 1.67+


IF (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	ADD_COMPILE_OPTIONS (-O3) # Needed for precompiled headers to work
endif()

ADD_COMPILE_DEFINITIONS (USE_SIMDE) # Enable OpenMP support in simde
ADD_COMPILE_DEFINITIONS (SIMDE_ENABLE_OPENMP) # Enable OpenMP support in simde
ADD_COMPILE_OPTIONS (-fopenmp-simd) # Enable OpenMP SIMD support
ADD_COMPILE_OPTIONS (-fnon-call-exceptions) # Allow signal handler to throw exception

ADD_COMPILE_OPTIONS (-Wno-deprecated-declarations -Wno-write-strings -Wno-multichar -Wno-cpp -Werror)
IF (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wno-aggressive-loop-optimizations)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-terminate -Wno-psabi" )
ELSEIF (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Help TBB figure out what compiler support for c++11 features
    # https://github.com/01org/tbb/issues/22
    string(REPLACE "." "0" TBB_USE_GLIBCXX_VERSION ${CMAKE_CXX_COMPILER_VERSION})
    message(STATUS "ADDING: -DTBB_USE_GLIBCXX_VERSION=${TBB_USE_GLIBCXX_VERSION}")
    add_definitions(-DTBB_USE_GLIBCXX_VERSION=${TBB_USE_GLIBCXX_VERSION})
ENDIF ()
