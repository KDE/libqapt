set(sourceslisttest_SRCS
    sourceslisttest.cpp
 )
set(transactionerrorhandlingtest_SRCS
    transactionerrorhandlingtest.cpp
 )

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src
    ${KDE4_INCLUDES}
    ${QT_INCLUDES}
)

add_executable(sourceslisttest ${sourceslisttest_SRCS})
add_executable(transactionerrorhandlingtest ${transactionerrorhandlingtest_SRCS})

configure_file(
    data/test1.list
    ${CMAKE_CURRENT_BINARY_DIR}/data/test1.list
    COPYONLY
)
configure_file(
    data/test2.list
    ${CMAKE_CURRENT_BINARY_DIR}/data/test2.list
    COPYONLY
)

target_link_libraries(sourceslisttest
                      -L../src/ qapt
                      ${QT_QTCORE_LIBRARIES}
                      ${QT_QTDBUS_LIBRARIES}
                      ${QT_QTTEST_LIBRARIES}
                      ${APTPKG_LIBRARIES}
                      ${XAPIAN_LIBRARIES}
                      debconf-kde)
target_link_libraries(transactionerrorhandlingtest
                      -L../src/ qapt
                      ${QT_QTCORE_LIBRARIES}
                      ${QT_QTDBUS_LIBRARIES}
                      ${QT_QTTEST_LIBRARIES}
                      ${APTPKG_LIBRARIES}
                      ${XAPIAN_LIBRARIES}
                      debconf-kde)

add_test(NAME sourceslisttest COMMAND sourceslisttest)
add_test(NAME transactionerrorhandlingtest COMMAND sourceslisttest)

