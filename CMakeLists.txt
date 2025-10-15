cmake_minimum_required(VERSION 3.5.0...3.31.2)

string(TIMESTAMP PROJECT_TIMESTAMP "%Y.%m.%d.%H%M" UTC)
project(DamageMeter VERSION ${PROJECT_TIMESTAMP} LANGUAGES C CXX)

file(GLOB_RECURSE SRCs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.[hc]pp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Util/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Agent/Agent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Gadget/Gadget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Gadget/GdAttackTarget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Player/Player.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Game/EventApi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/PropContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Char/ChCliContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Char/Character.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE/Game/Combat/Tracker/CombatEvent.cpp
)
find_package(imgui)
add_library(${PROJECT_NAME}
    SHARED
        ${SRCs}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/thirdparty
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GW2RE
)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        V_MAJOR=${PROJECT_VERSION_MAJOR}
        V_MINOR=${PROJECT_VERSION_MINOR}
        V_BUILD=${PROJECT_VERSION_PATCH}
        V_REVISION=$<IF:$<BOOL:${PROJECT_VERSION_TWEAK}>,${PROJECT_VERSION_TWEAK},0>
        REMOTE_URL="https://github.com/TyrianDeveloperCollective/GW2-DamageMeter"
)
target_link_libraries(${PROJECT_NAME}
    imgui::imgui
)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Personal_Combat_Metrics")
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "."
)
