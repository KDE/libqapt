project(qapt)

cmake_minimum_required(VERSION 2.6.0)
set(qapt_VERSION 0.1.0)

set(qapt_lib_SOVERSION 0)
set(qapt_lib_VERSION ${qapt_lib_SOVERSION}.1.0)

find_package(Qt4 REQUIRED)

# Find the required Libaries
include_directories(
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${QT_QTXML_INCLUDE_DIR}
    ${QT_QTGUI_INCLUDE_DIR}
    )

add_subdirectory(src)

configure_file(qapt.pc.in ${CMAKE_BINARY_DIR}/qapt.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/qapt.pc
    DESTINATION lib${LIB_SUFFIX}/pkgconfig
    )

# Add translations
# include(MacroOptionalAddSubdirectory)
# macro_optional_add_subdirectory(po)
