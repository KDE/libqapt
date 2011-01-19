project(qaptbatch)
cmake_minimum_required(VERSION 2.6.2)

if (NOT KDE4_FOUND)
    find_package(KDE4 REQUIRED)
    include(KDE4Defaults)
endif (NOT KDE4_FOUND)
include(FindPkgConfig REQUIRED)

add_subdirectory(qapt-batch)

if (GLIB2_FOUND)
    if (GSTREAMER_FOUND)
        add_subdirectory(qapt-gst-helper)
    endif (GSTREAMER_FOUND)
endif (GLIB2_FOUND)
