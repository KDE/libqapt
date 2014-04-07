find_package(DebconfKDE REQUIRED)

add_subdirectory(qapt-batch)
add_subdirectory(qapt-deb-installer)
add_subdirectory(qapt-deb-thumbnailer)
add_subdirectory(plasma-runner-installer)

find_package(GStreamer)

macro_log_feature(GSTREAMER_FOUND "GStreamer" ""
                  "http://gstreamer.freedesktop.org" FALSE "" "Used to build an optional GStreamer plugin installer")

if(GSTREAMER_FOUND)
    find_package(GLIB2)
    macro_log_feature(GLIB2_FOUND "GLib2" "" "http://www.gtk.org/download/" TRUE)

    if (GLIB2_FOUND)
        add_definitions(-fexceptions)
        add_definitions(-DGST_DISABLE_DEPRECATED)
        add_subdirectory(qapt-gst-helper)
    endif()
endif()
