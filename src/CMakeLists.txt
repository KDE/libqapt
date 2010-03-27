set(qapt_SRCS
    backend.cpp
    package.cpp
    )

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
    )

set(qapt_HDRS
    backend.h
    package.h
   )

qt4_wrap_cpp(qapt_SRCS ${qapt_HDRS})

# qt4_add_dbus_adaptor(dbusmenu_qt_SRCS
#     ${CMAKE_CURRENT_SOURCE_DIR}/org.ayatana.dbusmenu.xml
#     ${CMAKE_CURRENT_SOURCE_DIR}/dbusmenuexporter.h DBusMenuExporter
#     )

add_library(qapt SHARED ${qapt_SRCS})
set_target_properties(qapt PROPERTIES
    VERSION ${qapt_lib_VERSION}
    SOVERSION ${qapt_lib_SOVERSION}
    )

target_link_libraries(qapt
    ${QT_QTGUI_LIBRARIES}
    ${QT_QTDBUS_LIBRARIES}
    ${QT_QTXML_LIBRARIES}
    ${QT_QTCORE_LIBRARIES}
    apt-pkg
    )

install(TARGETS qapt
    LIBRARY DESTINATION lib${LIB_SUFFIX})

install(DIRECTORY .
    DESTINATION include/qapt
    FILES_MATCHING PATTERN "*.h"
    PATTERN "*_p.h" EXCLUDE
    )
