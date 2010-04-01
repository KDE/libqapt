project(qapt)

cmake_minimum_required(VERSION 2.6.0)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

set(qapt_VERSION 0.1.0)

set(qapt_lib_SOVERSION 0)
set(qapt_lib_VERSION ${qapt_lib_SOVERSION}.1.0)

configure_file(
       "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/cmake_uninstall.cmake.in"
       "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
       IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/qapt.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/qapt.pc
               @ONLY )

# This macro is defined in FindPolkitQt.cmake
macro(dbus_add_activation_system_service _sources)
    foreach (_i ${_sources})
        get_filename_component(_service_file ${_i} ABSOLUTE)
        string(REGEX REPLACE "\\.service.*$" ".service" _output_file ${_i})
        set(_target ${CMAKE_CURRENT_BINARY_DIR}/${_output_file})
        configure_file(${_service_file} ${_target})
        install(FILES ${_target} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services )
        #install(FILES ${_target} DESTINATION ${_install_dir})
    endforeach (_i ${ARGN})
endmacro(dbus_add_activation_system_service _sources)

find_package(Qt4 REQUIRED)
find_package(Automoc4 REQUIRED)
find_package(PolkitQt-1 REQUIRED)

# Find the required Libaries
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${POLKITQT-1_INCLUDE_DIR}
    )

add_subdirectory(src)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qapt.pc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/pkgconfig
        COMPONENT Devel )

# Add translations
# include(MacroOptionalAddSubdirectory)
# macro_optional_add_subdirectory(po)
