cmake_minimum_required(VERSION 2.8)
set(PROJNAME gl_path_rendering_CMYK)
Project(${PROJNAME})
Message(STATUS "-------------------------------")
Message(STATUS "Processing Project ${PROJNAME}:")

#####################################################################################
# offer the choice of having shared_sources as a sub-folder... good for packaging a sample
#
# if BASE_DIRECTORY not defined, it means this cmake file was called as the first entry point and not included
if(NOT BASE_DIRECTORY) # if not defined, it means this cmake file was called as the first entry point and not included
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shared_sources)
    Message(STATUS "found sub-folder shared_sources")
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/shared_sources/CMakeLists_include.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/shared_sources ${CMAKE_BINARY_DIR}/shared_sources)
  elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../shared_sources)
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)
    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../../shared_sources/CMakeLists_include.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../shared_sources ${CMAKE_BINARY_DIR}/shared_sources)
  else()
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../shared_sources/CMakeLists_include.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared_sources ${CMAKE_BINARY_DIR}/shared_sources)
  endif()
endif()
_add_project_definitions(${PROJNAME})

#####################################################################################
# additions from packages needed for this sample
# add refs  in LIBRARIES_OPTIMIZED
# add refs  in LIBRARIES_DEBUG
# add files in PACKAGE_SOURCE_FILES
#
_add_package_NSight()
_add_package_MFCUI()
#_add_package_AntTweakBar()
_add_package_OpenGLText()
#_add_package_nvFx()

#####################################################################################
# Source files for this project
#
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
#####################################################################################
# additional files from helpers
#
#LIST(APPEND COMMON_SOURCE_FILES 
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers_gl/GLSLProgram.h
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers_gl/GLSLProgram.cpp
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers_gl/WindowInertiaCamera.h
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers/TimeSampler.h
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers/InertiaCamera.h
#  ${BASE_DIRECTORY}/shared_sources/nv_math/nv_math.inl
#  ${BASE_DIRECTORY}/shared_sources/nv_math/nv_math.h
#  ${BASE_DIRECTORY}/shared_sources/nv_math/nv_math_types.h
#)
#####################################################################################
# Executable
#
if(WIN32 AND NOT GLUT_FOUND)
  add_definitions(/wd4267) #remove size_t to int warning
  add_definitions(/wd4996) #remove printf warning
  add_definitions(/wd4244) #remove double to float conversion warning
  add_definitions(/wd4305) #remove double to float truncation warning
  add_executable(${PROJNAME} WIN32 ${SOURCE_FILES} ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
else()
  add_executable(${PROJNAME} ${SOURCE_FILES} ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
endif()

#####################################################################################
# common source code needed for this sample
#
source_group(common FILES 
  ${COMMON_SOURCE_FILES}
)
#####################################################################################
# Linkage
#
target_link_libraries(${PROJNAME} optimized
    ${LIBRARIES_OPTIMIZED}
    ${PLATFORM_LIBRARIES}
    shared_sources
)
target_link_libraries(${PROJNAME} debug
    ${LIBRARIES_DEBUG}
    ${PLATFORM_LIBRARIES}
    shared_sources
)

#####################################################################################
# copies binaries that need to be put next to the exe files (ZLib, etc.)
#
_copy_binaries_to_target( ${PROJNAME} )
