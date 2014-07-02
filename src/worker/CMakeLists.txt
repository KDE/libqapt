include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

if ("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr") # If the prefix is usr, things will not be in /usr/etc/
    set(SYSCONF_INSTALL_DIR "/etc") # ...but rather in /etc
else ("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
    set(SYSCONF_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/etc") # Otherwise, trust the prefix
endif ("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")

set(qapt_worker_SRCS
    main.cpp
    aptlock.cpp
    aptworker.cpp
    transaction.cpp
    transactionadapter.cpp
    transactionqueue.cpp
    workeracquire.cpp
    workerdaemon.cpp
    workerinstallprogress.cpp
)

qt5_add_dbus_adaptor(qapt_worker_SRCS
    org.kubuntu.qaptworker2.xml
    workerdaemon.h
    WorkerDaemon
)

add_executable(qaptworker2
    ${qapt_worker_SRCS}
    ${qapt_worker_adaptor_SRCS}
)

# see our worker is pretty small :D
target_link_libraries(qaptworker2
    Qt5::Core
    ${POLKITQT-1_CORE_LIBRARY}
    util
    QApt
)

install(TARGETS qaptworker2 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# dbus_add_activation_system_service(org.kubuntu.qaptworker2.service.in)

find_package(KDE4)
if (KDE4_FOUND)
    include(KDE4Defaults)
    kde4_install_auth_actions(org.kubuntu.qaptworker2 qaptworker.actions)
else (KDE4_FOUND)
    install(FILES org.kubuntu.qaptworker2.policy DESTINATION ${CMAKE_INSTALL_PREFIX}/share/polkit-1/actions)
endif (KDE4_FOUND)

install(FILES org.kubuntu.qaptworker2.conf DESTINATION ${SYSCONF_INSTALL_DIR}/dbus-1/system.d/)
