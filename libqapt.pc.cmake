prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@CMAKE_INSTALL_PREFIX@
libdir=@CMAKE_INSTALL_PREFIX@/lib
includedir=@CMAKE_INSTALL_PREFIX@/include/libqapt

Name: libqapt
Description: Qt wrapper around libapt-pkg
Version: @qapt_VERSION@
Libs: -L${libdir} -lqapt
Cflags: -I${includedir}
