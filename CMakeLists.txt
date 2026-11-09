cmake_minimum_required(VERSION 3.20)
project(lgl-games-launcher VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(GNUInstallDirs)
include(CTest)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall>
                      $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
                      $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
                      $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif()

option(LGL_GAMES_LAUNCHER_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(LGL_GAMES_LAUNCHER_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.20)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.6)
pkg_check_modules(GLIBMM REQUIRED IMPORTED_TARGET glibmm-2.68)
pkg_check_modules(GIOMM REQUIRED IMPORTED_TARGET giomm-2.68)
pkg_check_modules(LIBSOUP REQUIRED IMPORTED_TARGET libsoup-3.0)
pkg_check_modules(JSONGLIB REQUIRED IMPORTED_TARGET json-glib-1.0)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkitgtk-6.0)
pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive)
find_package(SQLite3 REQUIRED)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

set(RESOURCE_XML ${PROJECT_SOURCE_DIR}/data/lgl-games-launcher.gresource.xml)
set(RESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/lgl-games-launcher-resources.c)
set(RESOURCE_DEPS ${PROJECT_SOURCE_DIR}/data/style.css
    ${PROJECT_SOURCE_DIR}/data/icons/lgl-games-launcher.png
    ${PROJECT_SOURCE_DIR}/data/icons/no-art.png)
add_custom_command(OUTPUT ${RESOURCE_C}
  COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target=${RESOURCE_C}
          --sourcedir=${PROJECT_SOURCE_DIR}/data ${RESOURCE_XML}
  DEPENDS ${RESOURCE_XML} ${RESOURCE_DEPS} VERBATIM)
set_source_files_properties(${RESOURCE_C} PROPERTIES COMPILE_OPTIONS "-w")

add_library(lgl_launcher_core STATIC
  src/core/paths.cpp
  src/core/settings.cpp
  src/core/valve_key_values.cpp
  src/core/steam.cpp
  src/core/faugus.cpp
  src/core/store.cpp
  src/core/library.cpp
  src/core/flags_store.cpp
  src/core/launch.cpp
  src/core/theme.cpp
  src/core/format.cpp
  src/core/artwork.cpp
  src/core/secret_store.cpp
  src/core/igdb.cpp
  src/core/epic.cpp
  src/core/ubisoft.cpp
  src/core/steam_achievements.cpp
)
target_include_directories(lgl_launcher_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(lgl_launcher_core PUBLIC PkgConfig::GLIBMM PkgConfig::GIOMM PkgConfig::LIBSOUP PkgConfig::JSONGLIB PkgConfig::LIBSECRET PkgConfig::LIBXML2 PkgConfig::LIBARCHIVE SQLite3::SQLite3)

add_executable(lgl-games-launcher
  ${RESOURCE_C}
  src/main.cpp
  src/ui/themewatcher.cpp
  src/ui/window.cpp
  src/ui/library_view.cpp
  src/ui/details_view.cpp
  src/ui/epic_login.cpp
  src/ui/ubisoft_login.cpp
)
target_compile_definitions(lgl-games-launcher PRIVATE
  LGL_GAMES_LAUNCHER_VERSION="${PROJECT_VERSION}")
target_link_libraries(lgl-games-launcher PRIVATE
  lgl_launcher_core PkgConfig::GTKMM PkgConfig::ADWAITA PkgConfig::WEBKIT)

install(TARGETS lgl-games-launcher RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/com.linuxgamerlife.lgl-games-launcher.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/com.linuxgamerlife.lgl-games-launcher.metainfo.xml
  DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
foreach(size IN ITEMS 48 64 128 256)
  install(FILES packaging/icons/${size}x${size}.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
    RENAME lgl-games-launcher.png)
endforeach()
install(FILES README.md CHANGELOG.md HELP.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/lgl-games-launcher)

if(BUILD_TESTING)
  message(STATUS "Tests are deferred for the Steam MVP; the maintainer tests manually.")
endif()
