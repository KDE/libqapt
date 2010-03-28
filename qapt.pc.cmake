prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@CMAKE_INSTALL_PREFIX@
libdir=@CMAKE_INSTALL_PREFIX@/src
includedir=@CMAKE_INSTALL_PREFIX@/include/qapt

Name: libqapt
Description: Qt wrapper around libapt-pkg
Version: @qaptt_VERSION@
Libs: -L${libdir} -lqapt
Cflags: -I${includedir}
