# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"plasma-runner-installer\")

add_library(krunner_installer MODULE installerrunner.cpp)

target_link_libraries(krunner_installer
    KF5::I18n
    KF5::Runner)

install(TARGETS krunner_installer DESTINATION ${PLUGIN_INSTALL_DIR} )
install(FILES plasma-runner-installer.desktop DESTINATION ${SERVICES_INSTALL_DIR})
