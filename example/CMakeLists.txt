project(qapttest)
cmake_minimum_required(VERSION 2.6.0)

find_package(KDE4 REQUIRED)
include(FindPkgConfig REQUIRED)
find_package(QApt REQUIRED)
include(KDE4Defaults)

set(qapttest_SRCS
    main.cpp
    qapttest.cpp
    cacheupdatewidget.cpp
    commitwidget.cpp
 )

include( FindPkgConfig )

include_directories( ${KDE4_INCLUDES} ${QT_INCLUDES} )

kde4_add_executable(qaptest ${qapttest_SRCS})

target_link_libraries(qaptest ${KDE4_KDEUI_LIBS}
                              debconf-kde
                              qapt)

install(TARGETS qaptest ${INSTALL_TARGETS_DEFAULT_ARGS} )

