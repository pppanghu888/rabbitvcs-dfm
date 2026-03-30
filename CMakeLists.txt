cmake_minimum_required(VERSION 3.10)
project(rabbitvcs-dfm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Menu grouping option
option(USE_ACT_GROUP_ID "Enable act_group_id for menu grouping" OFF)
if(USE_ACT_GROUP_ID)
    message(STATUS "Menu grouping: ENABLED")
else()
    message(STATUS "Menu grouping: DISABLED")
endif()

# Find Qt - prefer Qt6, fallback to Qt5
find_package(Qt6 QUIET COMPONENTS Core DBus Gui Widgets Concurrent)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    message(STATUS "Using Qt6")
    set(QT_TARGET_PREFIX Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core DBus Gui Widgets Concurrent)
    set(QT_VERSION_MAJOR 5)
    message(STATUS "Using Qt5")
    set(QT_TARGET_PREFIX Qt5)
endif()

find_package(PkgConfig REQUIRED)

# Find dfm-extension
pkg_check_modules(dfm-extension REQUIRED dfm-extension)

# Set source files
set(SRCS
    src/rabbitvcs-dfm-menu.cpp
    src/rabbitvcsdfmmenuplugin.cpp
    src/rabbitvcsmenuhelper.cpp
    src/statuschecker.cpp
    src/rabbitvcsdfmemblems.cpp
)

# Set header files
set(HEADERS
    src/rabbitvcsdfmmenuplugin.h
    src/rabbitvcsmenuhelper.h
    src/statuschecker.h
    src/rabbitvcsdfmemblems.h
)

# Create shared library
add_library(rabbitvcs-dfm SHARED ${SRCS} ${HEADERS})

# Link libraries
target_link_libraries(rabbitvcs-dfm
    PRIVATE
    ${QT_TARGET_PREFIX}::Core
    ${QT_TARGET_PREFIX}::DBus
    ${QT_TARGET_PREFIX}::Gui
    ${QT_TARGET_PREFIX}::Widgets
    ${QT_TARGET_PREFIX}::Concurrent
    ${dfm-extension_LIBRARIES}
)

# Include directories
target_include_directories(rabbitvcs-dfm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${dfm-extension_INCLUDE_DIRS}
)

# Compile definitions
target_compile_definitions(rabbitvcs-dfm PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_VERSION_MAJOR=${QT_VERSION_MAJOR}
    $<$<BOOL:${USE_ACT_GROUP_ID}>:RABBITVCS_USE_ACT_GROUP_ID=1>
)

# Installation
include(GNUInstallDirs)

# Plugin installation directory
set(DFM_EXT_PLUGIN_DIR ${CMAKE_INSTALL_FULL_LIBDIR}/dde-file-manager/plugins/extensions)

install(TARGETS rabbitvcs-dfm LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR})

# Install icon
install(FILES icon.png
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/48x48/apps
        OPTIONAL)

# Install translations
install(FILES translations/rabbitvcs-dfm_zh_CN.qm
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/rabbitvcs-dfm/translations
        OPTIONAL)

# Print configuration info
message(STATUS "Installation prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Plugin directory: ${DFM_EXT_PLUGIN_DIR}")
message(STATUS "dfm-extension include dirs: ${dfm-extension_INCLUDE_DIRS}")
message(STATUS "dfm-extension libraries: ${dfm-extension_LIBRARIES}")
message(STATUS "Qt version: ${QT_VERSION_MAJOR}")
