cmake_minimum_required(VERSION 3.25...3.30)
project(KeyGuide)

# Keep the established option names compatible with existing presets/caches.
option(KEYGUIDE_BUILD_TESTS
    "Build portable Vim/Blender tracker, queue and command tests" OFF)
option(KEYGUIDE_NATIVE_MACOS "Build the passive macOS overlay backend" ON)
set(KEYGUIDE_USE_MACOS OFF)
if (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND KEYGUIDE_NATIVE_MACOS
    AND NOT "${PLATFORM}" STREQUAL "Web" AND NOT ANDROID)
  set(KEYGUIDE_USE_MACOS ON)
  if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0"
        CACHE STRING "Minimum macOS for passive observation" FORCE)
  endif()
  enable_language(OBJC OBJCXX)
endif()

include(FetchContent)

# Always build the bundled raylib source as a static library. Do not silently
# substitute a system-installed shared library.
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)

# Make Windows release packages independent of the Visual C++ runtime DLLs.
if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY
      "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
set(RAYLIB_VERSION 6.0)
set(RAYLIB_ARCHIVE_BASE_URL
    "https://github.com/raysan5/raylib/archive"
)

FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL
        "${RAYLIB_ARCHIVE_BASE_URL}/refs/tags/${RAYLIB_VERSION}.tar.gz"
)

# Portable header-only libraries: JSON for settings / AI responses, TOML for keymap files.
set(NLOHMANN_JSON_VERSION 3.12.0)
FetchContent_Declare(
    nlohmann_json
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL
        "https://github.com/nlohmann/json/releases/download/v${NLOHMANN_JSON_VERSION}/json.tar.xz"
)

set(TOMLPLUSPLUS_VERSION 3.4.0)
FetchContent_Declare(
    tomlplusplus
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL
        "https://github.com/marzer/tomlplusplus/archive/refs/tags/v${TOMLPLUSPLUS_VERSION}.tar.gz"
)

FetchContent_MakeAvailable(raylib nlohmann_json tomlplusplus)

# HTTP client for AI explanation providers (Ollama, DigitalOcean). macOS ships libcurl and
# Linux distributions package it; Windows builds it against the native Schannel TLS stack.
option(KEYGUIDE_AI_NETWORK "Build the HTTP client used by AI explanation providers" ON)
set(KEYGUIDE_HAS_CURL OFF)
if (KEYGUIDE_AI_NETWORK AND NOT "${PLATFORM}" STREQUAL "Web" AND NOT ANDROID)
  if (WIN32)
    set(CURL_VERSION 8.23.0)
    string(REPLACE "." "_" CURL_TAG "${CURL_VERSION}")
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
    set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
    set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
    set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        curl
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL "https://github.com/curl/curl/releases/download/curl-${CURL_TAG}/curl-${CURL_VERSION}.tar.xz"
    )
    FetchContent_MakeAvailable(curl)
    set(KEYGUIDE_HAS_CURL ON)
  else()
    find_package(CURL QUIET)
    if (CURL_FOUND)
      set(KEYGUIDE_HAS_CURL ON)
    else()
      message(WARNING "libcurl not found; AI explanations will be unavailable. "
                      "Install libcurl development files (e.g. libcurl4-openssl-dev).")
    endif()
  endif()
endif()

# Our Project
add_executable(${PROJECT_NAME})
add_subdirectory(src)
target_link_libraries(${PROJECT_NAME}
    nlohmann_json::nlohmann_json tomlplusplus::tomlplusplus)
if (KEYGUIDE_HAS_CURL)
  target_link_libraries(${PROJECT_NAME} CURL::libcurl)
  target_compile_definitions(${PROJECT_NAME} PRIVATE KEYGUIDE_HAS_CURL=1)
endif()

if (WIN32)
  target_sources(
      ${PROJECT_NAME} PRIVATE
      packaging/windows/KeyGuide.rc
  )
endif()

# DEBUG_MODE_ACTIVATOR----------===================Debug

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

set_property(
    TARGET ${PROJECT_NAME}
    PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

set(ASSETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets")

if (NOT "${PLATFORM}" STREQUAL "Web")
  add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            ${ASSETS_SOURCE_DIR}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
endif()

#set(raylib_VERBOSE 1)
target_link_libraries(${PROJECT_NAME} raylib)

# Portable build-and-run entry point for terminals and editor tasks.
if (NOT "${PLATFORM}" STREQUAL "Web" AND NOT ANDROID)
  add_custom_target(run
      COMMAND $<TARGET_FILE:${PROJECT_NAME}>
      DEPENDS ${PROJECT_NAME}
      WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
      USES_TERMINAL
      VERBATIM
  )
endif()

# Web configuration
if ("${PLATFORM}" STREQUAL "Web")
  set(
      WEB_SHELL_FILE
      "${CMAKE_SOURCE_DIR}/packaging/web/minishell.html"
  )

  if (NOT EXISTS "${WEB_SHELL_FILE}")
    message(FATAL_ERROR "Web shell template not found: ${WEB_SHELL_FILE}")
  endif()

  set_target_properties(${PROJECT_NAME} PROPERTIES
          OUTPUT_NAME "index"
          SUFFIX ".html"
      )

  target_link_options(${PROJECT_NAME} PRIVATE
          "-sUSE_GLFW=3"
          "--preload-file"
          "${ASSETS_SOURCE_DIR}@assets"
          "--shell-file"
          "${WEB_SHELL_FILE}"
      )

endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE AND NOT "${PLATFORM}" STREQUAL "Web" AND NOT ANDROID)
  target_link_libraries(${PROJECT_NAME} "-framework IOKit")
  target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
  target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()

if (KEYGUIDE_USE_MACOS)
  target_link_libraries(${PROJECT_NAME}
      "-framework ApplicationServices" "-framework Carbon" "-framework Security")
  set_target_properties(${PROJECT_NAME} PROPERTIES
      OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED YES)
  set_source_files_properties(
      src/platform/mac/input_observer.mm
      src/platform/mac/overlay_window.mm
      src/platform/mac/overlay_menu.mm
      src/platform/mac/secret_store.mm
      PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  # package-macos assembles, signs and verifies KeyGuide.app in the build folder, then replaces
  # the copy in KEYGUIDE_APP_DIR. A stable identity keeps Input Monitoring granted across
  # rebuilds; "-" (ad hoc) needs a re-grant.
  set(KEYGUIDE_APP_DIR "/Applications" CACHE PATH "Folder package-macos installs KeyGuide.app into")
  if (KEYGUIDE_APP_DIR STREQUAL "" OR KEYGUIDE_APP_DIR MATCHES ";")
    # A ";" would split the path into several command arguments.
    message(FATAL_ERROR "KEYGUIDE_APP_DIR must name one folder without ';', e.g. /Applications")
  endif()
  if (NOT DEFINED KEYGUIDE_CODESIGN_IDENTITY)
    # First development / Developer ID identity in the keychain. Its SHA-1 stays in this
    # build's cache, so no personal certificate name is written to the repository.
    set(detected_identity "-")
    execute_process(COMMAND security find-identity -v -p codesigning
        OUTPUT_VARIABLE identities ERROR_QUIET)
    if (identities MATCHES "([0-9A-F]+) \"(Apple Development|Developer ID Application)")
      set(detected_identity "${CMAKE_MATCH_1}")
    endif()
  endif()
  set(KEYGUIDE_CODESIGN_IDENTITY "${detected_identity}" CACHE STRING
      "codesign identity (name or SHA-1) for KeyGuide.app; '-' signs ad hoc")
  if (KEYGUIDE_CODESIGN_IDENTITY STREQUAL "" OR KEYGUIDE_CODESIGN_IDENTITY MATCHES ";")
    message(FATAL_ERROR "KEYGUIDE_CODESIGN_IDENTITY must be one identity, or '-' for ad hoc")
  endif()
  # Staged in the build folder and removed after installing, so only one KeyGuide.app with
  # this bundle identifier stays registered with macOS.
  set(STAGING_BUNDLE_DIR "${CMAKE_BINARY_DIR}/package-macos/KeyGuide.app")
  set(APP_CONTENTS_DIR "${STAGING_BUNDLE_DIR}/Contents")
  set(MAC_PACKAGING_DIR "${CMAKE_SOURCE_DIR}/packaging/mac")
  add_custom_target(package-macos
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGING_BUNDLE_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${APP_CONTENTS_DIR}/MacOS"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${APP_CONTENTS_DIR}/Resources"
      COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PROJECT_NAME}>"
          "${APP_CONTENTS_DIR}/MacOS/KeyGuide"
      COMMAND "${CMAKE_COMMAND}" -E copy "${MAC_PACKAGING_DIR}/Info.plist"
          "${APP_CONTENTS_DIR}/Info.plist"
      COMMAND "${CMAKE_COMMAND}" -E copy "${MAC_PACKAGING_DIR}/KeyGuide.icns"
          "${APP_CONTENTS_DIR}/Resources/KeyGuide.icns"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ASSETS_SOURCE_DIR}"
          "${APP_CONTENTS_DIR}/Resources/assets"
      COMMAND codesign --force --sign "${KEYGUIDE_CODESIGN_IDENTITY}" "${STAGING_BUNDLE_DIR}"
      COMMAND codesign --verify --strict "${STAGING_BUNDLE_DIR}"
      COMMAND "${CMAKE_COMMAND}" "-DSTAGING=${STAGING_BUNDLE_DIR}"
          "-DDEST=${KEYGUIDE_APP_DIR}/KeyGuide.app"
          -P "${MAC_PACKAGING_DIR}/install_bundle.cmake"
      DEPENDS ${PROJECT_NAME} VERBATIM)
endif()

if (KEYGUIDE_BUILD_TESTS AND NOT "${PLATFORM}" STREQUAL "Web" AND NOT ANDROID)
  enable_testing()
  add_subdirectory(tests)
endif()
