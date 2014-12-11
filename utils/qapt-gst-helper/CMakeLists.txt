set(qapt-gst-helper_SRCS
    main.cpp
    GstMatcher.cpp
    PluginFinder.cpp
    PluginHelper.cpp
    PluginInfo.cpp)

include_directories(${GSTREAMER_INCLUDE_DIR}
    ${GLIB2_INCLUDE_DIR})

add_executable(qapt-gst-helper ${qapt-gst-helper_SRCS})

target_link_libraries(qapt-gst-helper
    ${GSTREAMER_LIBRARIES}
    ${GLIB2_LIBRARIES}
    KF5::CoreAddons
    KF5::I18n
    KF5::KIOCore
    KF5::WidgetsAddons
    KF5::WindowSystem
    QApt::Main)

install(TARGETS qapt-gst-helper DESTINATION ${LIBEXEC_INSTALL_DIR})
