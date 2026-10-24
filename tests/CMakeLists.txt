# tests/CMakeLists.txt
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(TRACKER_SOURCES
    ${SRC_DIR}/gameLogic/vim_tracker.cpp
    ${SRC_DIR}/gameLogic/vim_bindings.cpp)
set(APP_TRACKER_SOURCES ${TRACKER_SOURCES}
    ${SRC_DIR}/gameLogic/tracker.cpp
    ${SRC_DIR}/gameLogic/shortcut_bindings.cpp
    ${SRC_DIR}/gameLogic/aerospace_bindings.cpp
    ${SRC_DIR}/gameLogic/blender_tracker.cpp
    ${SRC_DIR}/gameLogic/blender_bindings.cpp)
# Guide sources compile without KEYGUIDE_HAS_CURL here, so tests never touch the network.
set(GUIDE_SOURCES
    ${SRC_DIR}/guide/ai_client.cpp
    ${SRC_DIR}/guide/catalog.cpp
    ${SRC_DIR}/guide/explanation_parser.cpp
    ${SRC_DIR}/guide/explanations.cpp
    ${SRC_DIR}/settings/app_paths.cpp
    ${SRC_DIR}/settings/persistence.cpp)
set(TEST_TARGETS
    KeyGuideTrackerTests
    KeyGuideBlenderTrackerTests
    KeyGuideInputQueueTests
    KeyGuideCommandPipelineTests
    KeyGuideAeroSpaceConfigTests
    KeyGuideGuideTests)
add_executable(KeyGuideTrackerTests
    vim_tracker_tests.cpp ${TRACKER_SOURCES})
add_executable(KeyGuideBlenderTrackerTests
    blender_tracker_tests.cpp ${APP_TRACKER_SOURCES})
add_executable(KeyGuideInputQueueTests input_queue_tests.cpp)
add_executable(KeyGuideCommandPipelineTests
    command_pipeline_tests.cpp platform_stub.cpp ${APP_TRACKER_SOURCES}
    ${SRC_DIR}/gameLogic/input.cpp
    ${SRC_DIR}/gameLogic/command_execution.cpp
    ${SRC_DIR}/gameLogic/tick_logic.cpp
    ${SRC_DIR}/registry/commands.cpp
    ${SRC_DIR}/registry/registry.cpp
    ${GUIDE_SOURCES})
add_executable(KeyGuideAeroSpaceConfigTests
    aerospace_config_tests.cpp ${SRC_DIR}/keymaps/aerospace_config.cpp)
target_link_libraries(KeyGuideAeroSpaceConfigTests
    PRIVATE tomlplusplus::tomlplusplus)
add_executable(KeyGuideGuideTests
    guide_tests.cpp ${APP_TRACKER_SOURCES}
    ${SRC_DIR}/guide/ai_client.cpp
    ${SRC_DIR}/guide/catalog.cpp
    ${SRC_DIR}/guide/explanation_parser.cpp)
target_link_libraries(KeyGuideGuideTests PRIVATE nlohmann_json::nlohmann_json)
find_package(Threads REQUIRED)
target_link_libraries(KeyGuideInputQueueTests
    PRIVATE Threads::Threads)
target_link_libraries(KeyGuideCommandPipelineTests
    PRIVATE raylib nlohmann_json::nlohmann_json)
foreach(target ${TEST_TARGETS})
    target_include_directories(${target}
        PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${target} PRIVATE cxx_std_20)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
add_custom_target(KeyGuideTests DEPENDS ${TEST_TARGETS})
