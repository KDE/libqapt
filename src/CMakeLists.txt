set(libqapt_SRCS
    backend.cpp
    package.cpp
    group.cpp
    )

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
    )

add_subdirectory(worker)

set(libqapt_HDRS
    backend.h
    package.h
    group.h
    globals.h
   )

qt4_wrap_cpp(libqapt_SRCS ${libqapt_HDRS})

add_library(libqapt SHARED ${libqapt_SRCS})
set_target_properties(libqapt PROPERTIES
    VERSION ${libqapt_lib_VERSION}
    SOVERSION ${libqapt_lib_SOVERSION}
    )

target_link_libraries(libqapt
    ${QT_QTGUI_LIBRARIES}
    ${QT_QTDBUS_LIBRARIES}
    ${QT_QTXML_LIBRARIES}
    ${QT_QTCORE_LIBRARIES}
    apt-pkg
    )

install(TARGETS libqapt
    LIBRARY DESTINATION lib${LIB_SUFFIX})

install(FILES
    backend.h
    package.h
    group.h
    globals.h

    DESTINATION
    include/libqapt COMPONENT Devel)
