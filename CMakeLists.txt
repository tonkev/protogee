cmake_minimum_required(VERSION 3.8)

project(protogee)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_MODULE_PATH "${protogee_SOURCE_DIR}/cmake")

set(RR_EMBED_KERNELS ON CACHE BOOL "Embed CL kernels into binary module")
set(RR_ALLOW_CPU_DEVICES OFF CACHE BOOL "Allows CPU Devices")
set(RR_USE_OPENCL ON CACHE BOOL "Use OpenCL for GPU hit testing")
set(RR_USE_EMBREE OFF CACHE BOOL "Use Intel(R) Embree for CPU hit testing")
set(RR_USE_VULKAN OFF CACHE BOOL "Use vulkan for GPU hit testing")
set(RR_NO_TESTS ON CACHE BOOL "Don't add any unit tests and remove any test functionality from the library")
set(RR_ENABLE_STATIC ON CACHE BOOL "Create static libraries rather than dynamic")
set(RR_SHARED_CALC OFF CACHE BOOL "Link Calc(compute abstraction layer) dynamically")
set(RR_ENABLE_RAYMASK OFF CACHE BOOL "Enable ray masking in intersection kernels")
set(RR_TUTORIALS OFF CACHE BOOL "Add tutorials projects")
set(RR_SAFE_MATH ON CACHE BOOL "use safe math")
set(RR_ENABLE_BACKFACE_CULL ON CACHE BOOL "Enable backface culling in intersection kernels")

find_package(OpenCL REQUIRED)
include_directories(${OpenCL_INCLUDE_DIRS})
link_directories(${OpenCL_LIBRARY})

find_package(OpenGL REQUIRED)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

find_package(SDL2_image REQUIRED)
include_directories(${SDL2_IMAGE_INCLUDE_DIRS})

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIR})

find_package (GLM REQUIRED)
include_directories(${GLM_INCLUDE_DIR})

add_subdirectory(RadeonRays_SDK)
include_directories(RadeonRays_SDK/RadeonRays/include)

include_directories(include)
file(GLOB SOURCES "src/*.cpp")

include_directories(inih inih/cpp)
set(SOURCES ${SOURCES} inih/ini.c)
set(SOURCES ${SOURCES} inih/cpp/INIReader.cpp)

include_directories(tinyobjloader)

include_directories(halton)
set(SOURCES ${SOURCES} halton/halton.cpp)

add_executable(protogee ${SOURCES})
set_target_properties(protogee PROPERTIES CXX_STANDARD 17)
target_link_libraries(protogee ${SDL2_LIBRARY} ${SDL2_IMAGE_LIBRARIES} ${GLEW_LIBRARY} ${OPENGL_gl_LIBRARY} ${OpenCL_LIBRARY} RadeonRays)