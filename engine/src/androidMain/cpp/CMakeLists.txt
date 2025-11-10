cmake_minimum_required(VERSION 3.22.1)
project(vkrenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compile GLSL to SPIR-V using NDK-provided glslc
set(SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders)
file(MAKE_DIRECTORY ${ASSETS_DIR})

# Host tag detection for glslc path inside NDK
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	set(HOST_TAG "windows-x86_64")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
	# Apple Silicon and Intel both use mac host tools path in recent NDKs
	set(HOST_TAG "darwin-x86_64")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	set(HOST_TAG "linux-x86_64")
endif()

set(GLSLC "${CMAKE_ANDROID_NDK}/shader-tools/${HOST_TAG}/glslc")

set(GLSL_SOURCES
	${SHADERS_DIR}/triangle.vert
	${SHADERS_DIR}/triangle.frag
)

set(SPV_OUTPUTS)
foreach(src ${GLSL_SOURCES})
	get_filename_component(fname ${src} NAME)
	set(out ${ASSETS_DIR}/${fname}.spv)
	add_custom_command(
		OUTPUT ${out}
		COMMAND ${GLSLC} -c ${src} -o ${out}
		DEPENDS ${src}
		COMMENT "Compiling GLSL ${fname} to SPIR-V"
		VERBATIM
	)
	list(APPEND SPV_OUTPUTS ${out})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPV_OUTPUTS})

add_library(vkrenderer SHARED
	src/vkrenderer.cpp
	src/vkrenderer.h
)

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

target_include_directories(vkrenderer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(vkrenderer PRIVATE VK_USE_PLATFORM_ANDROID_KHR)

target_link_libraries(vkrenderer
	${log-lib}
	${android-lib}
	${vulkan-lib}
)

add_dependencies(vkrenderer compile_shaders)


