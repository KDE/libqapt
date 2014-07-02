set(qapt-deb-installer_SRCS
    main.cpp
    DebCommitWidget.cpp
    DebInstaller.cpp
    DebViewer.cpp
    ChangesDialog.cpp)

if (CMAKE_COMPILER_IS_GNUCXX)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif (CMAKE_COMPILER_IS_GNUCXX)

add_executable(qapt-deb-installer ${qapt-deb-installer_SRCS})

target_link_libraries(qapt-deb-installer
    KF5::CoreAddons
    KF5::I18n
    KF5::KIOCore
    KF5::TextWidgets
    KF5::WidgetsAddons
    QApt)

install(TARGETS qapt-deb-installer ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS qapt-deb-installer.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR})
