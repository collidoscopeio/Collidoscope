#__LINUX_ALSA__ Basic
cmake_minimum_required( VERSION 2.8 FATAL_ERROR )
set( CMAKE_VERBOSE_MAKEFILE on )

project( CollidoscopeApp HOMEPAGE_URL http://collidoscope.io)

get_filename_component( CINDER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Cinder" ABSOLUTE )
get_filename_component( APP_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE )

set( INC_DIR ${APP_PATH}/include )
set( SRC_DIR ${APP_PATH}/src )

set( SRC_FILES
    ${INC_DIR}/AudioEngine.h
    ${INC_DIR}/BufferToWaveRecorderNode.h
    ${INC_DIR}/Chunk.h
    ${INC_DIR}/Config.h
    ${INC_DIR}/DrawInfo.h
    ${INC_DIR}/Envelopes.h
    ${INC_DIR}/Log.h
    ${INC_DIR}/Messages.h
    ${INC_DIR}/MIDI.h
    ${INC_DIR}/Oscilloscope.h
    ${INC_DIR}/ParticleController.h
    ${INC_DIR}/PGranular.h
    ${INC_DIR}/PGranularNode.h
    ${INC_DIR}/Resources.h
    ${INC_DIR}/RingBufferPack.h
    ${INC_DIR}/RtMidi.h
    ${INC_DIR}/Wave.h
    ${SRC_DIR}/CollidoscopeApp.cpp
    ${SRC_DIR}/AudioEngine.cpp
    ${SRC_DIR}/BufferToWaveRecorderNode.cpp
    ${SRC_DIR}/Chunk.cpp
    ${SRC_DIR}/Config.cpp
    ${SRC_DIR}/Log.cpp
    ${SRC_DIR}/MIDI.cpp
    ${SRC_DIR}/PGranularNode.cpp
    ${SRC_DIR}/RtMidi.cpp
    ${SRC_DIR}/Wave.cpp
)

include( "${CINDER_PATH}/proj/cmake/modules/cinderMakeApp.cmake" )

add_definitions(-DNUM_WAVES=2)
add_definitions(-DUSE_PARTICLES)

if(WIN32)
    add_definitions(-D__WINDOWS_MM__)
    set( LIBS "winmm")
elseif(APPLE)
    add_definitions(-D__MACOSX_CORE__)
    set(LIBS "-framework CoreMIDI -framework CoreAudio -framework CoreFoundation")
else() #LINUX
    add_definitions(-D__LINUX_ALSA__)
    set( LIBS "-lasound -latomic -ljack")
endif()

ci_make_app(
    SOURCES     ${SRC_FILES}
    INCLUDES    ${INC_DIR}
    LIBRARIES   ${LIBS}
    CINDER_PATH ${CINDER_PATH}
)

