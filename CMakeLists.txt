cmake_minimum_required(VERSION 3.16)
project(vanillavso-backend VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "5.240.0")

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(ECMQtDeclareLoggingCategory)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(GenerateExportHeader)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Qml Test)
find_package(KF6 REQUIRED COMPONENTS
    Auth
    CoreAddons
    I18n
    ItemModels
    KIO
    XmlGui
)
find_package(KF6Auth REQUIRED CONFIG)
find_package(QCoro6 REQUIRED COMPONENTS Core)

set(DISCOVER_PLUGIN_IID ${PROJECT_VERSION})
set(DISCOVER_NOTIFIER_IID ${PROJECT_VERSION})
string(PREPEND DISCOVER_PLUGIN_IID "org.kde.discover.")
string(APPEND DISCOVER_PLUGIN_IID ".AbstractResourcesBackendFactory")
string(PREPEND DISCOVER_NOTIFIER_IID "org.kde.discover.")
string(APPEND DISCOVER_NOTIFIER_IID ".BackendNotifierModule")

configure_file(discover/DiscoverConfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/DiscoverConfig.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(discover/libdiscover)

set(vanillavsobackend_SRCS
    VanillaVSOAuthActionFactory.cpp
    VanillaVSOAuthActionFactory.h
    VanillaVSOBackend.cpp
    VanillaVSOBackend.h
    VanillaVSOResource.cpp
    VanillaVSOResource.h
    VanillaVSOTransaction.cpp
    VanillaVSOTransaction.h
)

ecm_qt_declare_logging_category(
    vanillavsobackend_SRCS
    HEADER libdiscover_vanillavso_debug.h
    IDENTIFIER VANILLAVSO_LOG
    CATEGORY_NAME org.kde.plasma.libdiscover.backend.vanillavso
    DESCRIPTION "libdiscover (backend - Vanilla OS)"
    DEFAULT_SEVERITY Debug
)

add_library(vanillavso-backend MODULE ${vanillavsobackend_SRCS})

target_link_libraries(vanillavso-backend
    PRIVATE
        Qt::Core
        KF6::AuthCore
        KF6::CoreAddons
        KF6::I18n
        Discover::Common
)

add_executable(vanillavso_kauth_helper
    VanillaVSOKAuthHelper.cpp
    VanillaVSOKAuthHelper.h
    org.kde.discover.vanillavsobackend.actions
)

set_source_files_properties(
    org.kde.discover.vanillavsobackend.actions
    PROPERTIES HEADER_FILE_ONLY ON
)

ecm_qt_declare_logging_category(
    vanillavso_kauth_helper_SRCS
    HEADER libdiscover_vanillavso_helper_debug.h
    IDENTIFIER VANILLAVSO_HELPER_LOG
    CATEGORY_NAME org.kde.plasma.libdiscover.backend.vanillavso.helper
    DESCRIPTION "libdiscover (backend - Vanilla OS, KAuth helper)"
    DEFAULT_SEVERITY Debug
)

target_sources(vanillavso_kauth_helper PRIVATE ${vanillavso_kauth_helper_SRCS})

target_link_libraries(vanillavso_kauth_helper
    PRIVATE
        Qt::Core
        KF6::AuthCore
)

kauth_install_actions(org.kde.discover.vanillavsobackend org.kde.discover.vanillavsobackend.actions)
kauth_install_helper_files(vanillavso_kauth_helper org.kde.discover.vanillavsobackend root)

install(TARGETS vanillavso-backend
    DESTINATION ${KDE_INSTALL_PLUGINDIR}/discover)

install(TARGETS vanillavso_kauth_helper
    DESTINATION ${KAUTH_HELPER_INSTALL_DIR})