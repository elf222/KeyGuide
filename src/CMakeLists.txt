file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    *.c
    *.cpp
    *.cc
    *.cxx
)

file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS
    *.h
    *.hpp
    *.hh
    *.hxx
)

list(FILTER SOURCE_FILES EXCLUDE REGEX "/platform/(mac|fallback)/")
if (KEYGUIDE_USE_MACOS)
  list(APPEND SOURCE_FILES
      platform/mac/input_observer.mm
      platform/mac/overlay_window.mm
      platform/mac/overlay_menu.mm
      platform/mac/secret_store.mm)
else()
  list(APPEND SOURCE_FILES platform/fallback/platform.cpp)
endif()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(
    ${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
