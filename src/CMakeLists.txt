set(qapt_SRCS
    backend.cpp
    cache.cpp
    package.cpp
    )

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
    )

add_subdirectory(worker)

set(qapt_MOC_HDRS
    backend.h
    cache.h
   )

set(qapt_HDRS
    package.h
    globals.h
    )

qt4_wrap_cpp(qapt_SRCS ${qapt_MOC_HDRS})

qt4_add_dbus_interface(qapt_SRCS
                       worker/org.kubuntu.qaptworker.xml
                       workerdbus)

add_library(qapt SHARED ${qapt_SRCS})
set_target_properties(qapt PROPERTIES
    VERSION ${qapt_lib_VERSION}
    SOVERSION ${qapt_lib_SOVERSION}
    )

target_link_libraries(qapt
    ${QT_QTDBUS_LIBRARIES}
    ${QT_QTCORE_LIBRARIES}
    ${APTPKG_LIBRARIES}
    ${XAPIAN_LIBRARIES}
    )

install(TARGETS qapt
    LIBRARY DESTINATION lib${LIB_SUFFIX})

install(FILES
    backend.h
    cache.h
    package.h
    globals.h

    DESTINATION
    include/libqapt COMPONENT Devel)
