set(qapt-gst-helper_SRCS
    main.cpp
    GstMatcher.cpp
    PluginFinder.cpp
    PluginHelper.cpp
    PluginInfo.cpp)

include_directories(${KDE4_INCLUDES}
                    ${QT_INCLUDES}
                    ${QTGSTREAMER_INCLUDE_DIR})

kde4_add_executable(qapt-gst-helper ${qapt-gst-helper_SRCS})

target_link_libraries(qapt-gst-helper
                      ${KDE4_KDEUI_LIBS}
                      ${QTGSTREAMER_LIBRARY}
                      qapt)

install(TARGETS qapt-gst-helper DESTINATION ${LIBEXEC_INSTALL_DIR})
