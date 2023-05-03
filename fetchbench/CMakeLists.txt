cmake_minimum_required(VERSION 3.7)

# -------------------------------------------------------------------------
# arch. detection based on https://stackoverflow.com/a/12024211
set(archdetect_c_code "
#if defined(__aarch64__)
    #if defined(__APPLE__)
        #error cmake_ARCH arm_apple
    #else
        #error cmake_ARCH arm
    #endif
#elif defined(__x86_64__)
    #error cmake_ARCH x86_64
#endif
#error cmake_ARCH unknown
")
function(target_architecture output_var)
    file(WRITE "${CMAKE_BINARY_DIR}/arch.c" "${archdetect_c_code}")
    enable_language(C)
    try_run(
        run_result_unused
        compile_result_unused
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_BINARY_DIR}/arch.c"
        COMPILE_OUTPUT_VARIABLE ARCH
        CMAKE_FLAGS CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
    )
    string(REGEX MATCH "cmake_ARCH ([a-zA-Z0-9_]+)" ARCH "${ARCH}")
    string(REPLACE "cmake_ARCH " "" ARCH "${ARCH}")
    if (NOT ARCH)
        set(ARCH unknown)
    endif()
    set(${output_var} "${ARCH}" PARENT_SCOPE)
endfunction()

# identify the local architecture
set(ARCH unknown)
target_architecture(ARCH)
# -------------------------------------------------------------------------


# set up project
set(PROJ_NAME "fetchbench")
project(${PROJ_NAME} LANGUAGES CXX C ASM)

# add debug symbols to all outputs
set(CMAKE_BUILD_TYPE Debug)

# load Threads package (for -pthread)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# include 3rd-party libraries
include_directories(3rdparty/json11)

# ================ Main executable ================

# collect source files
file(GLOB SOURCES "src/*.cc" "3rdparty/json11/*.cpp")

# define resulting binary
add_executable(${PROJ_NAME} ${SOURCES})

# COMPILER FLAGS
# enable warnings
target_compile_options(${PROJ_NAME} PRIVATE "-Wall")
# optimization
target_compile_options(${PROJ_NAME} PRIVATE "-Os")
# enable C++17 support
target_compile_options(${PROJ_NAME} PRIVATE "-std=c++17")

# link it with -pthread
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

# optinally, create statically linked binary
if (LINK_STATIC)
	message("linking statically")
	target_link_libraries(${PROJ_NAME} PRIVATE -static)
endif()

# ================ Pointer array test ================

if (ARCH STREQUAL "x86_64")
	add_executable(prefetch-test-parr "src/parr/parr_x86.S" "src/parr/parr.c")
	target_compile_options(prefetch-test-parr PRIVATE -DFLUSHING)
	add_executable(prefetch-test-pchase "src/pchase/pchase_x86.S" "src/pchase/pchase.c")
	target_compile_options(prefetch-test-pchase PRIVATE -DFLUSHING)
elseif (ARCH STREQUAL "arm")
	add_executable(prefetch-test-parr "src/parr/parr_arm.S" "src/parr/parr.c")
	target_compile_options(prefetch-test-parr PRIVATE -DFLUSHING)
	add_executable(prefetch-test-pchase "src/pchase/pchase_arm.S" "src/pchase/pchase.c")
	target_compile_options(prefetch-test-pchase PRIVATE -DFLUSHING)
elseif (ARCH STREQUAL "arm_apple")
	add_executable(prefetch-test-parr "src/parr/parr_arm.S" "src/parr/parr.c")
	target_compile_options(prefetch-test-parr PRIVATE -DFLUSHING)
	add_executable(prefetch-test-pchase "src/pchase/pchase_arm.S" "src/pchase/pchase.c")
	target_compile_options(prefetch-test-pchase PRIVATE -DFLUSHING)
else()
	message(FATAL_ERROR "Architecture not supported")
endif()

# set compiler flags for AoP/Pchase test binary
target_link_libraries(prefetch-test-parr PRIVATE -static)
target_link_libraries(prefetch-test-parr PRIVATE Threads::Threads)

target_link_libraries(prefetch-test-pchase PRIVATE -static)
target_link_libraries(prefetch-test-pchase PRIVATE Threads::Threads)

# ================ Doxygen ================
# (see https://vicrucann.github.io/tutorials/quick-cmake-doxygen/)

# add option for creating doxygen documentation (and enable it by default)
option(BUILD_DOC "Build documentation" ON)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
	# set input and output files
	set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
	set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

	# request to configure the file
	configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
	message(STATUS "Doxygen build started")

	# note the option ALL which allows to build the docs together with the application
	add_custom_target( doc_doxygen 
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Generating documentation with Doxygen"
		VERBATIM )

else (DOXYGEN_FOUND)
	message(STATUS "Doxygen needs to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)
