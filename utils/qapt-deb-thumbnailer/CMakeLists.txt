set(qapt-deb-thumbnailer_SRCS DebThumbnailer.cpp)

add_library(debthumbnailer MODULE ${qapt-deb-thumbnailer_SRCS})

target_link_libraries(debthumbnailer ${KDE4_KIO_LIBS} qapt)

install(TARGETS debthumbnailer DESTINATION ${PLUGIN_INSTALL_DIR} )

install(FILES  debthumbnailer.desktop DESTINATION ${SERVICES_INSTALL_DIR})
