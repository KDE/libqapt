set(qaptbatch_SRCS
    main.cpp
    qaptbatch.cpp
    detailswidget.cpp)

qt4_add_dbus_interface(qaptbatch_SRCS
                       ../../src/worker/org.kubuntu.qaptworker.xml
                       workerdbus)

include_directories(${KDE4_INCLUDES} ${QT_INCLUDES})

kde4_add_executable(qapt-batch ${qaptbatch_SRCS})

target_link_libraries(qapt-batch ${KDE4_KDEUI_LIBS} qapt)

install(TARGETS qapt-batch ${INSTALL_TARGETS_DEFAULT_ARGS})