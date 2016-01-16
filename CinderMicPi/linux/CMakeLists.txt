# MultichannelOutput
cmake_minimum_required( VERSION 2.8 FATAL_ERROR )
set( CMAKE_VERBOSE_MAKEFILE on )

get_filename_component( CINDER_DIR "/home/ulysses/development/cinder-android" ABSOLUTE )
include( ${CINDER_DIR}/linux/cmake/Cinder.cmake )

project( MultichannelOutput )

get_filename_component( SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE )
get_filename_component( INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include" ABSOLUTE )

if( NOT TARGET cinder${CINDER_LIB_SUFFIX} )
    find_package( cinder REQUIRED
        PATHS /home/ulysses/development/cinder-android/linux/${CMAKE_BUILD_TYPE}/${CINDER_OUT_DIR_PREFIX}
        $ENV{Cinder_DIR}/linux/${CMAKE_BUILD_TYPE}/${CINDER_OUT_DIR_PREFIX}
    )
endif()

# Use PROJECT_NAME since CMAKE_PROJET_NAME returns the top-level project name.
set( EXE_NAME ${PROJECT_NAME} )

set( SRC_FILES
	${SRC_DIR}/MultichannelOutputApp.cpp
	/home/ulysses/development/cinder-android/samples/_audio/common/AudioDrawUtils.cpp
)

add_executable( "${EXE_NAME}" ${SRC_FILES} )

target_include_directories(
	"${EXE_NAME}"
	PUBLIC ${INC_DIR}
)

SET(GCC_COVERAGE_LINK_FLAGS    "-lwiringPi")
SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}" )

target_link_libraries( "${EXE_NAME}" cinder${CINDER_LIB_SUFFIX} )
target_link_libraries( "${EXE_NAME}" pulse )
