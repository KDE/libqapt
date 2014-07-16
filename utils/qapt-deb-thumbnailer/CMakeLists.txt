add_library(debthumbnailer MODULE DebThumbnailer.cpp)

target_link_libraries(debthumbnailer
    KF5::KIOWidgets
    QApt::Main)

message(WARNING "make sure thumbnailer actually works")

install(TARGETS debthumbnailer DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES debthumbnailer.desktop DESTINATION ${SERVICES_INSTALL_DIR})
