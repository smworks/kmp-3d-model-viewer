cmake_minimum_required(VERSION 3.22.1)
project(vkrenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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


