set(qapt_SRCS
    backend.cpp
    cache.cpp
    package.cpp
    config.cpp
    history.cpp
    debfile.cpp
    dependencyinfo.cpp
    changelog.cpp
    transaction.cpp
    transactiondbus.cpp
    downloadprogress.cpp
    markingerrorinfo.cpp
    sourceentry.cpp
    sourceslist.cpp)

add_subdirectory(worker)

qt5_add_dbus_interface(qapt_SRCS
                       worker/org.kubuntu.qaptworker2.xml
                       workerdbus)

add_library(QApt SHARED ${qapt_SRCS})

set_target_properties(QApt
    PROPERTIES
    VERSION ${QAPT_VERSION_STRING}
    SOVERSION ${QAPT_SOVERSION}
    EXPORT_NAME QApt)

target_link_libraries(QApt
    PUBLIC
        Qt5::Widgets
        ${APTPKG_LIBRARIES}
    PRIVATE
        Qt5::DBus
        ${XAPIAN_LIBRARIES})

install(TARGETS QApt EXPORT QAptTargets LIBRARY DESTINATION ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

ecm_generate_headers(QAPT_CAMEL_CASE_HEADERS
    HEADER_NAMES
        Backend
        Changelog
        Config
        DebFile
        DependencyInfo
        DownloadProgress
        Globals
        History
        MarkingErrorInfo
        Package
        SourceEntry
        SourcesList
        Transaction

  REQUIRED_HEADERS QAPT_HEADERS
  PREFIX qapt)

install(FILES ${QAPT_HEADERS} DESTINATION include/qapt COMPONENT Devel)
install(FILES ${QAPT_CAMEL_CASE_HEADERS} DESTINATION include/QApt COMPONENT Devel)
