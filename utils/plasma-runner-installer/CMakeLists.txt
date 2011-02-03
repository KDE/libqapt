include_directories(${KDE4_INCLUDES} ${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(krunner_installer_SRCS
    installerrunner.cpp
)

kde4_add_plugin(krunner_installer ${krunner_installer_SRCS})
target_link_libraries(krunner_installer ${KDE4_KIO_LIBS} ${KDE4_PLASMA_LIBS} )

install(TARGETS krunner_installer DESTINATION ${PLUGIN_INSTALL_DIR} )

install(FILES plasma-runner-installer.desktop DESTINATION ${SERVICES_INSTALL_DIR})

