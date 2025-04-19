CMAKE_MINIMUM_REQUIRED(VERSION 3.30)
project (lowtis)

include (ExternalProject)

set (RUN_ENVIRONMENT "Workstation" CACHE TYPE STRING)
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

set (CMAKE_CXX_LINK_FLAGS "-O3")
set (CMAKE_CXX_FLAGS_RELEASE "-O3")
set (CMAKE_CXX_FLAGS_DEBUG "-ggdb")
set (CMAKE_DEBUG_POSTFIX "-g")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
if (APPLE)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if (LIBDVID_INCLUDE)
    include_directories(AFTER ${LIBDVID_INCLUDE_DIR})
endif()

# Find Boost using CONFIG mode
find_package(Boost REQUIRED CONFIG COMPONENTS thread system)
include_directories(AFTER ${Boost_INCLUDE_DIRS})

# Set boost libraries using modern target-based approach
set(boost_LIBS Boost::thread Boost::system)

if (LIBDVID_WRAP_PYTHON)
    find_package(Boost REQUIRED CONFIG COMPONENTS python)
    set(boostpython_LIB Boost::python)
endif()

FIND_LIBRARY(DVIDCPP_LIBRARY dvidcpp)

FIND_LIBRARY(JSON_LIBRARIES jsoncpp)
if (NOT JSON_LIBRARIES)
    message(FATAL_ERROR "*** Could not find jsoncpp library ***")
endif()

set (support_LIBS ${DVIDCPP_LIBRARY} ${boost_LIBS} ${JSON_LIBRARIES})

include_directories (BEFORE ${CMAKE_SOURCE_DIR}/lowtis ${CMAKE_SOURCE_DIR})

if (NOT MSVC)
    # The -fPIC flag is necessary for "relocatable" code that might be included in an .so
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# Compile lowtis library components
add_library (lowtis SHARED 
             src/BlockCache.cpp
             src/BlockFetchFactory.cpp
             src/BlockFetch.cpp
             src/DVIDBlockFetch.cpp
             src/GoogleBlockFetch.cpp
             src/lowtis.cpp)

target_link_libraries (lowtis ${support_LIBS})

# config file for CMake FIND_PACKAGE command
set (lowtis_version "0.1.0")
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}")
export(TARGETS lowtis FILE "${PROJECT_BINARY_DIR}/lowtisTargets.cmake")

CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/config/lowtisConfig.cmake.in
    ${PROJECT_BINARY_DIR}/lowtisConfig.cmake
    @ONLY IMMEDIATE)
CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/config/lowtisConfigVersion.cmake.in
    ${PROJECT_BINARY_DIR}/lowtisConfigVersion.cmake
    @ONLY IMMEDIATE)

# installation for library
INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/lowtis DESTINATION include)
INSTALL(TARGETS lowtis DESTINATION lib)
INSTALL(FILES ${PROJECT_BINARY_DIR}/lowtisConfig.cmake
${PROJECT_BINARY_DIR}/lowtisConfigVersion.cmake ${PROJECT_BINARY_DIR}/lowtisTargets.cmake
DESTINATION lib${LIB_SUFFIX}/lowtis)



