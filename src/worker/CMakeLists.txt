
install(FILES org.kubuntu.qaptworker.policy DESTINATION ${SHARE_INSTALL_PREFIX}/polkit-1/actions/)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

#--------Helper Application

# This macro is defined in FindPolkitQt.cmake
macro(dbus_add_activation_system_service _sources)
    foreach (_i ${_sources})
        get_filename_component(_service_file ${_i} ABSOLUTE)
        string(REGEX REPLACE "\\.service.*$" ".service" _output_file ${_i})
        set(_target ${CMAKE_CURRENT_BINARY_DIR}/${_output_file})
        configure_file(${_service_file} ${_target})
        install(FILES ${_target} DESTINATION ${SHARE_INSTALL_PREFIX}/dbus-1/system-services )
        #install(FILES ${_target} DESTINATION ${_install_dir})
    endforeach (_i ${ARGN})
endmacro(dbus_add_activation_system_service _sources)

set(qapt_helper_SRCS
    main.cpp
    worker.cpp
)

set(qapt_helper_HDRS
    worker.h
)

# qt4_add_dbus_adaptor(qapt_helper_SRCS
#     org.kubuntu.qaptworker.xml
#     worker.cpp
#     AptWorker
# )

QT4_WRAP_CPP(qapt_helper_MOC_SRCS
    worker.h
)

add_executable(qapt-helper
    ${qapt_helper_SRCS}
    ${qapt_helper_MOC_SRCS}
)

# see our helper is pretty small :D
target_link_libraries(qapt-helper
    ${QT_QTCORE_LIBRARY}
    polkit-qt-core-1
    qapt
)

install(TARGETS qapt-helper DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

dbus_add_activation_system_service(org.kubuntu.qaptworker.service.in)

install(FILES org.qt.policykit.examples.conf DESTINATION ${SYSCONF_INSTALL_DIR}/dbus-1/system.d)
