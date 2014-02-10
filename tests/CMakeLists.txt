set(CMAKE_AUTOMOC TRUE)
include(FindPkgConfig REQUIRED)
find_package(QApt REQUIRED)

set(sourceslisttest_SRCS
    sourceslisttest.cpp
 )

include( FindPkgConfig )

include_directories( ../src ${KDE4_INCLUDES} ${QT_INCLUDES} )

add_executable(sourceslisttest ${sourceslisttest_SRCS})

target_link_libraries(sourceslisttest
                      -L../src/ qapt
                      ${QT_QTCORE_LIBRARIES}
                      ${QT_QTDBUS_LIBRARIES}
                      ${QT_QTTEST_LIBRARIES}
                      ${APTPKG_LIBRARIES}
                      ${XAPIAN_LIBRARIES}
                      debconf-kde)
add_test(NAME sourceslisttest COMMAND sourceslisttest)

