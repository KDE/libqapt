project(qapt)
cmake_minimum_required(VERSION 2.8.12)

set(PROJECT_VERSION_MAJOR 3)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 5)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}" )
set(PROJECT_SOVERSION ${PROJECT_VERSION_MAJOR})

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
include(KDECMakeSettings NO_POLICY_SCOPE)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMGeneratePriFile) # This needs InstallDirs

# Turn exceptions on
kde_enable_exceptions()

set(REQUIRED_QT_VERSION 5.8.0) # Used in QAptConfig
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED DBus Widgets)

find_package(Xapian REQUIRED)
find_package(AptPkg REQUIRED)

find_package(PolkitQt5-1 0.112 REQUIRED)

# Find the required Libaries
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${POLKITQT-1_INCLUDE_DIR}
    ${XAPIAN_INCLUDE_DIR}
    ${APTPKG_INCLUDE_DIR})

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX QAPT
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/qaptversion.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/QAptConfigVersion.cmake"
                  SOVERSION ${PROJECT_SOVERSION})

set(QAPT_WORKER_VERSION ${PROJECT_SOVERSION})
set(QAPT_WORKER_RDN org.kubuntu.qaptworker)
set(QAPT_WORKER_RDN_VERSIONED ${QAPT_WORKER_RDN}${QAPT_WORKER_VERSION})

# For forwarding into C++ convert them into properly excaped strings.
set(QAPT_WORKER_VERSION_STRING \"${QAPT_WORKER_VERSION}\")
set(QAPT_WORKER_RDN_VERSIONED_STRING \"${QAPT_WORKER_RDN_VERSIONED}\")

add_definitions(-DQAPT_WORKER_VERSION_STRING=${QAPT_WORKER_VERSION_STRING})
add_definitions(-DQAPT_WORKER_RDN_VERSIONED_STRING=${QAPT_WORKER_RDN_VERSIONED_STRING})

# Also forward version for utils.
add_definitions(-DCMAKE_PROJECT_VERSION=\"${PROJECT_VERSION}\")

set(KF5_DEP_VERSION "5.0.0")

# Only used to install worker actions
find_package(KF5Auth ${KF5_DEP_VERSION})

# Used for utils
find_package(KF5CoreAddons ${KF5_DEP_VERSION})
find_package(KF5I18n ${KF5_DEP_VERSION})
find_package(KF5KIO ${KF5_DEP_VERSION})
find_package(KF5Runner ${KF5_DEP_VERSION})
find_package(KF5TextWidgets ${KF5_DEP_VERSION})
find_package(KF5WidgetsAddons ${KF5_DEP_VERSION})
find_package(KF5WindowSystem ${KF5_DEP_VERSION})
find_package(KF5IconThemes ${KF5_DEP_VERSION})

find_package(DebconfKDE 1.0)
find_package(GStreamer 1.0)
find_package(GLIB2 2.0)

if (KF5CoreAddons_FOUND
        AND KF5I18n_FOUND
        AND KF5KIO_FOUND
        AND KF5Runner_FOUND
        AND KF5TextWidgets_FOUND
        AND KF5WidgetsAddons_FOUND
        AND KF5WindowSystem_FOUND
        AND KF5IconThemes_FOUND
        AND DebconfKDE_FOUND)
    set(WITH_UTILS true)
endif()

if (WITH_UTILS AND GSTREAMER_FOUND AND GLIB2_FOUND)
    set(WITH_GSTREAMER true)
    add_definitions(${GSTREAMER_DEFINITIONS})
endif()

add_feature_info(qapt-utils WITH_UTILS "Runtime utilities using KDE frameworks")
add_feature_info(qapt-gst-helper WITH_GSTREAMER "GStreamer codec helper util")

message(WARNING "gettext and tr in the same source is insanely tricky, maybe we should give some ki18n to qapt...")
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
        FILE QAptTargets.cmake
        NAMESPACE QApt:: )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qaptversion.h
        DESTINATION include/qapt
        COMPONENT Devel)

add_subdirectory(autotests)
add_subdirectory(src)

if(WITH_UTILS)
    add_subdirectory(utils)

    #Do not remove or modify these.  The release script substitutes in for these
    #comments with appropriate doc and translation directories.
    #PO_SUBDIR
    #DOC_SUBDIR
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libqapt.pc.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libqapt.pc
    @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libqapt.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT Devel)

ecm_generate_pri_file(BASE_NAME QApt
    LIB_NAME QApt
    DEPS "core widgets dbus"
    FILENAME_VAR PRI_FILENAME
    INCLUDE_INSTALL_DIR include/QApt)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
