set(qaptbatch_SRCS
    main.cpp
    qaptbatch.cpp
    detailswidget.cpp
)

add_executable(qapt-batch ${qaptbatch_SRCS})

target_link_libraries(qapt-batch
    KF5::CoreAddons
    KF5::I18n
    KF5::KIOCore
    KF5::WidgetsAddons
    KF5::WindowSystem
    QApt::Main)

install(TARGETS qapt-batch ${INSTALL_TARGETS_DEFAULT_ARGS})
