find_package(DebconfKDE REQUIRED)

add_subdirectory(qapt-batch)
add_subdirectory(qapt-deb-installer)
add_subdirectory(qapt-deb-thumbnailer)
add_subdirectory(plasma-runner-installer)

if (QTGSTREAMER_FOUND)
    add_definitions(-fexceptions)
    add_definitions(-DGST_DISABLE_DEPRECATED)
    add_subdirectory(qapt-gst-helper)
endif (QTGSTREAMER_FOUND)
