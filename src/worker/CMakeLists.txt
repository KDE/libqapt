include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src)

set(qapt_worker_SRCS
    main.cpp
    aptlock.cpp
    aptworker.cpp
    transaction.cpp
    transactionadapter.cpp
    transactionqueue.cpp
    workeracquire.cpp
    workerdaemon.cpp
    workerinstallprogress.cpp)

qt5_add_dbus_adaptor(qapt_worker_SRCS
    org.kubuntu.qaptworker2.xml
    workerdaemon.h
    WorkerDaemon)

add_executable(qaptworker2
    ${qapt_worker_SRCS}
    ${qapt_worker_adaptor_SRCS})

# see our worker is pretty small :D
target_link_libraries(qaptworker2
    Qt5::Core
    Qt5::DBus
    ${POLKITQT-1_CORE_LIBRARY}
    util
    QApt)

message(WARNING "worker randomly links against -lutil...")

install(TARGETS qaptworker2 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

configure_file(org.kubuntu.qaptworker2.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/org.kubuntu.qaptworker2.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kubuntu.qaptworker2.service
        DESTINATION ${DBUS_SYSTEM_SERVICES_INSTALL_DIR})

message(WARNING "kde4 dependent cmake stuff")
find_package(KDE4)
if (KDE4_FOUND)
    include(KDE4Defaults)
    kde4_install_auth_actions(org.kubuntu.qaptworker2 qaptworker.actions)
else (KDE4_FOUND)
    install(FILES org.kubuntu.qaptworker2.policy DESTINATION ${CMAKE_INSTALL_PREFIX}/share/polkit-1/actions)
endif (KDE4_FOUND)

install(FILES org.kubuntu.qaptworker2.conf DESTINATION ${SYSCONF_INSTALL_DIR}/dbus-1/system.d/)
