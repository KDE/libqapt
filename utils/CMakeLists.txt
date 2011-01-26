project(qaptbatch)
cmake_minimum_required(VERSION 2.6.2)

if (NOT KDE4_FOUND)
    find_package(KDE4 REQUIRED)
    include(KDE4Defaults)
endif (NOT KDE4_FOUND)
include(FindPkgConfig REQUIRED)

add_subdirectory(qapt-batch)

if (QTGSTREAMER_FOUND)
    add_definitions(-fexceptions)
    add_definitions(-DGST_DISABLE_DEPRECATED)
    add_subdirectory(qapt-gst-helper)
endif (QTGSTREAMER_FOUND)
