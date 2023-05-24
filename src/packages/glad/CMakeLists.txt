cmake_minimum_required (VERSION 3.10)
project (glad C)


add_library(glad 
	src/gl.c
	src/egl.c
	src/gles1.c
	src/gles2.c
	src/wgl.c
	include/glad/gl.h
	include/glad/egl.h
	include/glad/gles1.h
	include/glad/gles2.h
	include/glad/wgl.h
	include/KHR/khrplatform.h
	include/EGL/eglplatform.h
	)

target_include_directories(glad PUBLIC include/)

set_target_properties(glad PROPERTIES FOLDER packages)
