set(qapt-deb-installer_SRCS
    main.cpp
    DebCommitWidget.cpp
    DebInstaller.cpp
    DebViewer.cpp
    ChangesDialog.cpp)

include_directories(${KDE4_INCLUDES} ${QT_INCLUDES})

if (CMAKE_COMPILER_IS_GNUCXX)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif (CMAKE_COMPILER_IS_GNUCXX)

kde4_add_executable(qapt-deb-installer ${qapt-deb-installer_SRCS})

target_link_libraries(qapt-deb-installer ${KDE4_KDEUI_LIBS} ${DEBCONF_KDE_LIB} qapt)

install(TARGETS qapt-deb-installer ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS qapt-deb-installer.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR})
