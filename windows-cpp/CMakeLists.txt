cmake_minimum_required(VERSION 3.20)
project(asset_tracker VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ASSET_TRACKER_TESTS "Build the core test executable" OFF)

add_compile_definitions(NOMINMAX ASSET_TRACKER_VERSION="${PROJECT_VERSION}")

# vcpkg toolchain should be set via -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
# Pdf is the Qt PDF add-on: Maintenance Tool -> Additional Libraries -> Qt PDF.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Concurrent Pdf)
find_package(OpenSSL REQUIRED)
find_package(libzip CONFIG REQUIRED)

qt_standard_project_setup()

qt_add_resources(APP_RESOURCES resources.qrc)

# The version has one source: project() above. The installer includes this file.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version.nsh"
     "!define VERSION \"${PROJECT_VERSION}\"\n")

# Everything except main.cpp, so the tests link the same code the app runs.
qt_add_library(asset_core STATIC
    src/backup.cpp        src/backup.h
    src/config.cpp        src/config.h
    src/crypto.cpp        src/crypto.h
    src/csvexport.cpp     src/csvexport.h
    src/database.cpp      src/database.h
    src/firebaseclient.cpp src/firebaseclient.h
    src/icons.cpp         src/icons.h
    src/models.cpp        src/models.h
    src/report.cpp        src/report.h
    src/secretstore.cpp   src/secretstore.h
    src/storage.cpp       src/storage.h
    src/sync.cpp          src/sync.h
    src/aboutdialog.cpp   src/aboutdialog.h
    src/assetform.cpp     src/assetform.h
    src/assetlist.cpp     src/assetlist.h
    src/busy.cpp          src/busy.h
    src/helpdialog.cpp    src/helpdialog.h
    src/imageviewer.cpp   src/imageviewer.h
    src/insurerinfodialog.cpp src/insurerinfodialog.h
    src/mainwindow.cpp    src/mainwindow.h
    src/previewpanel.cpp  src/previewpanel.h
    src/syncdialog.cpp    src/syncdialog.h
    src/theme.cpp         src/theme.h
    src/toolbar.cpp       src/toolbar.h
    src/updatechecker.cpp src/updatechecker.h
    src/updatehistorydialog.cpp src/updatehistorydialog.h
)

target_include_directories(asset_core PUBLIC src)

target_link_libraries(asset_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Pdf
    OpenSSL::Crypto
    libzip::zip
)

# crypt32: DPAPI (CryptProtectData) for stored files and passwords.
target_link_libraries(asset_core PUBLIC crypt32)

# app.rc embeds app.ico into the .exe so Windows shows the correct icon.
qt_add_executable(asset_tracker WIN32
    src/main.cpp
    installer/app.rc
    ${APP_RESOURCES}
)
target_link_libraries(asset_tracker PRIVATE asset_core)

# After each build: deploy Qt runtime DLLs and copy vcpkg DLLs next to the exe
# so the output directory is always self-contained and installer-ready.
get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

add_custom_command(TARGET asset_tracker POST_BUILD
    COMMAND "${QT_BIN_DIR}/windeployqt.exe"
        "$<IF:$<CONFIG:Debug>,--debug,--release>"
        "$<TARGET_FILE:asset_tracker>"
    COMMENT "Deploying Qt runtime DLLs..."
    VERBATIM
)
# windeployqt only looks in the main Qt kit; this covers a separately installed add-on.
add_custom_command(TARGET asset_tracker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::Pdf>" "$<TARGET_FILE_DIR:asset_tracker>"
)
add_custom_command(TARGET asset_tracker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
        "$<TARGET_FILE_DIR:asset_tracker>"
    COMMENT "Copying vcpkg runtime DLLs..."
)

if(ASSET_TRACKER_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(asset_tracker_tests tests/tst_core.cpp)
    target_link_libraries(asset_tracker_tests PRIVATE asset_core Qt6::Test)
endif()
