add_subdirectory(qapt-batch)
add_subdirectory(qapt-deb-installer)
# add_subdirectory(qapt-deb-thumbnailer)
# add_subdirectory(plasma-runner-installer)

find_package(GStreamer)

if(GSTREAMER_FOUND)
    find_package(GLIB2)

    if (GLIB2_FOUND)
        add_definitions(-fexceptions)
        add_definitions(-DGST_DISABLE_DEPRECATED)
        add_subdirectory(qapt-gst-helper)
    endif()
endif()
