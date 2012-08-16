project(libqapt)

cmake_minimum_required(VERSION 2.6.0)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

set(qapt_VERSION 1.4.0)

set(qapt_lib_SOVERSION 1)
set(qapt_lib_VERSION ${qapt_lib_SOVERSION}.4.0)

configure_file(
       "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/cmake_uninstall.cmake.in"
       "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
       IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libqapt.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/libqapt.pc
               @ONLY )

configure_file(qaptversion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/qaptversion.h)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qaptversion.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/libqapt)

# This macro is defined in FindPolkitQt.cmake
macro(dbus_add_activation_system_service _sources)
    foreach (_i ${_sources})
        get_filename_component(_service_file ${_i} ABSOLUTE)
        string(REGEX REPLACE "\\.service.*$" ".service" _output_file ${_i})
        set(_target ${CMAKE_CURRENT_BINARY_DIR}/${_output_file})
        configure_file(${_service_file} ${_target})
        install(FILES ${_target} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services )
        #install(FILES ${_target} DESTINATION ${_install_dir})
    endforeach (_i ${ARGN})
endmacro(dbus_add_activation_system_service _sources)

find_package(Qt4 REQUIRED)
find_package(Automoc4 REQUIRED)
find_package(Xapian REQUIRED)
find_package(AptPkg REQUIRED)
find_package(QtGStreamer)

set(POLKITQT-1_MIN_VERSION "0.98.1")
find_package(PolkitQt-1 REQUIRED)

# Find the required Libaries
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${POLKITQT-1_INCLUDE_DIR}
    ${XAPIAN_INCLUDE_DIR}
    ${APTPKG_INCLUDE_DIR}
    )

MACRO(NORMALIZE_VERSION _requested_version _normalized_version)
    STRING(REGEX MATCH "[^0-9]*[0-9]+\\.[0-9]+\\.[0-9]+.*" _threePartMatch "${_requested_version}")
    if (_threePartMatch)
    # parse the parts of the version string
        STRING(REGEX REPLACE "[^0-9]*([0-9]+)\\.[0-9]+\\.[0-9]+.*" "\\1" _major_vers "${_requested_version}")
        STRING(REGEX REPLACE "[^0-9]*[0-9]+\\.([0-9]+)\\.[0-9]+.*" "\\1" _minor_vers "${_requested_version}")
        STRING(REGEX REPLACE "[^0-9]*[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" _patch_vers "${_requested_version}")
    else (_threePartMatch)
        STRING(REGEX REPLACE "([0-9]+)\\.[0-9]+" "\\1" _major_vers "${_requested_version}")
        STRING(REGEX REPLACE "[0-9]+\\.([0-9]+)" "\\1" _minor_vers "${_requested_version}")
        set(_patch_vers "0")
    endif (_threePartMatch)

    # compute an overall version number which can be compared at once
    MATH(EXPR ${_normalized_version} "${_major_vers}*10000 + ${_minor_vers}*100 + ${_patch_vers}")
ENDMACRO(NORMALIZE_VERSION)

MACRO(MACRO_CHECK_RANGE_INCLUSIVE_LOWER _lower_limit _value _upper_limit _ok)
   if (${_value} LESS ${_lower_limit})
      set( ${_ok} FALSE )
  elseif (${_value} EQUAL ${_lower_limit})
      set( ${_ok} TRUE )
  elseif (${_value} EQUAL ${_upper_limit})
      set( ${_ok} FALSE )
  elseif (${_value} GREATER ${_upper_limit})
      set( ${_ok} FALSE )
  else (${_value} LESS ${_lower_limit})
      set( ${_ok} TRUE )
  endif (${_value} LESS ${_lower_limit})
ENDMACRO(MACRO_CHECK_RANGE_INCLUSIVE_LOWER)

MACRO(MACRO_ENSURE_VERSION requested_version found_version var_too_old)
    NORMALIZE_VERSION( ${requested_version} req_vers_num )
    NORMALIZE_VERSION( ${found_version} found_vers_num )

    if (found_vers_num LESS req_vers_num)
        set( ${var_too_old} FALSE )
    else (found_vers_num LESS req_vers_num)
        set( ${var_too_old} TRUE )
    endif (found_vers_num LESS req_vers_num)

ENDMACRO(MACRO_ENSURE_VERSION)

MACRO(MACRO_ENSURE_VERSION2 requested_version2 found_version2 var_too_old2)
    MACRO_ENSURE_VERSION( ${requested_version2} ${found_version2} ${var_too_old2})
ENDMACRO(MACRO_ENSURE_VERSION2)

MACRO(MACRO_ENSURE_VERSION_RANGE min_version found_version max_version var_ok)
    NORMALIZE_VERSION( ${min_version} req_vers_num )
    NORMALIZE_VERSION( ${found_version} found_vers_num )
    NORMALIZE_VERSION( ${max_version} max_vers_num )

    MACRO_CHECK_RANGE_INCLUSIVE_LOWER( ${req_vers_num} ${found_vers_num} ${max_vers_num} ${var_ok})
ENDMACRO(MACRO_ENSURE_VERSION_RANGE)

if (CMAKE_COMPILER_IS_GNUCXX)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

   include(CheckCXXCompilerFlag)

   # visibility support
   check_cxx_compiler_flag(-fvisibility=hidden __HAVE_GCC_VISIBILITY)
   set( __HAVE_GCC_VISIBILITY ${__HAVE_GCC_VISIBILITY} CACHE BOOL "GCC support for hidden visibility")

   # get the gcc version
   exec_program(${CMAKE_C_COMPILER} ARGS ${CMAKE_C_COMPILER_ARG1} --version OUTPUT_VARIABLE _gcc_version_info)

   string (REGEX MATCH "[345]\\.[0-9]\\.[0-9]" _gcc_version "${_gcc_version_info}")
   # gcc on mac just reports: "gcc (GCC) 3.3 20030304 ..." without the patch level, handle this here:
   if (NOT _gcc_version)
      string (REGEX MATCH ".*\\(GCC\\).* ([34]\\.[0-9]) .*" "\\1.0" _gcc_version "${gcc_on_macos}")
      if (gcc_on_macos)
        string (REGEX REPLACE ".*\\(GCC\\).* ([34]\\.[0-9]) .*" "\\1.0" _gcc_version "${_gcc_version_info}")
      endif (gcc_on_macos)
   endif (NOT _gcc_version)

   if (_gcc_version)
      macro_ensure_version("4.1.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_1)
      macro_ensure_version("4.2.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_2)
      macro_ensure_version("4.3.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_3)
   endif (_gcc_version)

   # save a little by making local statics not threadsafe
   # ### do not enable it for older compilers, see
   # ### http://gcc.gnu.org/bugzilla/show_bug.cgi?id=31806
   if (GCC_IS_NEWER_THAN_4_3)
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
   endif (GCC_IS_NEWER_THAN_4_3)

   set(_GCC_COMPILED_WITH_BAD_ALLOCATOR FALSE)
   if (GCC_IS_NEWER_THAN_4_1)
      exec_program(${CMAKE_C_COMPILER} ARGS ${CMAKE_C_COMPILER_ARG1} -v OUTPUT_VARIABLE _gcc_alloc_info)
      string(REGEX MATCH "(--enable-libstdcxx-allocator=mt)" _GCC_COMPILED_WITH_BAD_ALLOCATOR "${_gcc_alloc_info}")
   endif (GCC_IS_NEWER_THAN_4_1)

   if (__HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR AND NOT WIN32)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
      # check that Qt defines Q_DECL_EXPORT as __attribute__ ((visibility("default")))
      # if it doesn't and KDE compiles with hidden default visibiltiy plugins will break
      set(_source "#include <QtCore/QtGlobal>\n int main()\n {\n #ifndef QT_VISIBILITY_AVAILABLE \n #error QT_VISIBILITY_AVAILABLE is not available\n #endif \n }\n")
      set(_source_file ${CMAKE_BINARY_DIR}/CMakeTmp/check_qt_visibility.cpp)
      file(WRITE "${_source_file}" "${_source}")
      set(_include_dirs "-DINCLUDE_DIRECTORIES:STRING=${QT_INCLUDES}")

      try_compile(_compile_result ${CMAKE_BINARY_DIR} ${_source_file} CMAKE_FLAGS "${_include_dirs}" COMPILE_OUTPUT_VARIABLE _compile_output_var)

      if(NOT _compile_result)
         message(FATAL_ERROR "Qt compiled without support for -fvisibility=hidden. This will break plugins and linking of some applications. Please fix your Qt installation.")
      endif(NOT _compile_result)

      if (GCC_IS_NEWER_THAN_4_2)
          set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
      endif (GCC_IS_NEWER_THAN_4_2)
   else (__HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR AND NOT WIN32)
      set (__HAVE_GCC_VISIBILITY 0)
   endif (__HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR AND NOT WIN32)

endif (CMAKE_COMPILER_IS_GNUCXX)

add_subdirectory(src)
add_subdirectory(includes)

find_package(KDE4)
if (KDE4_FOUND)
    include(KDE4Defaults)
    add_subdirectory(utils)

    #Do not remove or modify these.  The release script substitutes in for these
    #comments with appropriate doc and translation directories.
    #PO_SUBDIR
    #DOC_SUBDIR

macro_log_feature(KDE4_FOUND "KDE" "Development libraries for the KDE Platform"
                  "http://kde.org" FALSE "" "Used to build optional utilities that use QApt")
macro_log_feature(QTGSTREAMER_FOUND "QtGStreamer" "Qt wrapper around the GStreamer multimedia framework"
                  "http://gstreamer.freedesktop.org" FALSE "" "Used to build an optional GStreamer plugin installer")

macro_display_feature_log()
endif (KDE4_FOUND)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libqapt.pc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/pkgconfig
        COMPONENT Devel )
install(FILES ${CMAKE_SOURCE_DIR}/cmake/modules/QAptConfig.cmake
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/QApt/
        COMPONENT Devel )
