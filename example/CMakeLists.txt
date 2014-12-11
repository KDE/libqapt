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

add_executable(qaptest ${qapttest_SRCS})

target_link_libraries(qaptest ${KDE4_KDEUI_LIBS}
                              ${KDE4_KIO_LIBS}
                              ${QAPT_LIBRARY}
                              DebconfKDE::Main)

install(TARGETS qaptest ${INSTALL_TARGETS_DEFAULT_ARGS} )

