project(libqapt)

cmake_minimum_required(VERSION 2.8.12)

find_package(ECM 0.0.14 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH
    ${ECM_MODULE_PATH}
    ${ECM_KDE_MODULE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include(ECMGenerateHeaders)
include(ECMPackageConfigHelpers)
include(ECMPoQmTools)
include(ECMSetupVersion)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

# Turn exceptions on
kde_enable_exceptions()

set(REQUIRED_QT_VERSION 5.2.0) # Used in QAptConfig
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED DBus Widgets)

find_package(Xapian REQUIRED)
find_package(AptPkg REQUIRED)

# set(POLKITQT-1_MIN_VERSION "0.98.1")
# find_package(PolkitQt-1 REQUIRED)

# Find the required Libaries
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${POLKITQT-1_INCLUDE_DIR}
    ${XAPIAN_INCLUDE_DIR}
    ${APTPKG_INCLUDE_DIR})

ecm_setup_version(3.0.0 VARIABLE_PREFIX QAPT
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/qaptversion.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/QAptConfigVersion.cmake"
                        SOVERSION 3)

if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ecm_install_po_files_as_qm(po)
endif()

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}/QApt")

ecm_configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/QAptConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/QAptConfig.cmake"
    INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR})

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/QAptConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/QAptConfigVersion.cmake"
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel)
install(EXPORT QAptTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE QAptTargets.cmake)#  NAMESPACE KF5:: )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qaptversion.h
        DESTINATION include/qapt
        COMPONENT Devel)

# This macro is defined in FindPolkitQt.cmake
# macro(dbus_add_activation_system_service _sources)
#     foreach (_i ${_sources})
#         get_filename_component(_service_file ${_i} ABSOLUTE)
#         string(REGEX REPLACE "\\.service.*$" ".service" _output_file ${_i})
#         set(_target ${CMAKE_CURRENT_BINARY_DIR}/${_output_file})
#         configure_file(${_service_file} ${_target})
#         install(FILES ${_target} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services )
#         #install(FILES ${_target} DESTINATION ${_install_dir})
#     endforeach (_i ${ARGN})
# endmacro(dbus_add_activation_system_service _sources)

add_subdirectory(src)
# add_subdirectory(includes)
# add_subdirectory(tests)

find_package(KF5CoreAddons 5.0.0 REQUIRED)
find_package(KF5I18n 5.0.0 REQUIRED)
find_package(KF5KIO 5.0.0 REQUIRED)
find_package(KF5TextWidgets 5.0.0 REQUIRED)
find_package(KF5WidgetsAddons 5.0.0 REQUIRED)
find_package(KF5WindowSystem 5.0.0 REQUIRED)

find_package(DebconfKDE REQUIRED)


# if (KDE4_FOUND)
#     include(KDE4Defaults)
    add_subdirectory(utils)

    #Do not remove or modify these.  The release script substitutes in for these
    #comments with appropriate doc and translation directories.
    #PO_SUBDIR
    #DOC_SUBDIR

# macro_log_feature(KDE4_FOUND "KDE" "Development libraries for the KDE Platform"
#                   "http://kde.org" FALSE "" "Used to build optional utilities that use QApt")
# 
# endif (KDE4_FOUND)

# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libqapt.pc.cmake
#     ${CMAKE_CURRENT_BINARY_DIR}/libqapt.pc
#     @ONLY
# )
# 
# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libqapt.pc
#         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/pkgconfig
#         COMPONENT Devel )

# add_test(NAME sourceslisttest COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/sourceslisttest)
# add_test(NAME transactionerrorhandlingtest COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/transactionerrorhandlingtest)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
