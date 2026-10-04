cmake_minimum_required(VERSION 3.22)

# ── MetaModule SDK path ──────────────────────────────────────────────
# Can be overridden via cmake variable or environment variable.
if(NOT "${METAMODULE_SDK_DIR}" STREQUAL "")
    message("METAMODULE_SDK_DIR set by CMake variable: ${METAMODULE_SDK_DIR}")
elseif(DEFINED ENV{METAMODULE_SDK_DIR})
    set(METAMODULE_SDK_DIR "$ENV{METAMODULE_SDK_DIR}")
    message("METAMODULE_SDK_DIR set by environment variable: ${METAMODULE_SDK_DIR}")
else()
    set(METAMODULE_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../garmire/metamodule-plugin-sdk")
    message("METAMODULE_SDK_DIR set to default: ${METAMODULE_SDK_DIR}")
endif()

include(${METAMODULE_SDK_DIR}/plugin.cmake)

# ── Project ──────────────────────────────────────────────────────────
project(Garmire VERSION 2.25.0 DESCRIPTION "Garmire Laser Trajectory & Color Control System" LANGUAGES C CXX ASM)

add_library(Garmire STATIC)

# ── Source files ─────────────────────────────────────────────────────
set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

target_sources(Garmire PRIVATE
    ${SOURCE_DIR}/src/plugin.cpp
    ${SOURCE_DIR}/src/Chromance.cpp
    ${SOURCE_DIR}/src/Ants.cpp
    ${SOURCE_DIR}/src/HsvRgb.cpp
    ${SOURCE_DIR}/src/OklchRgb.cpp
    ${SOURCE_DIR}/src/Rgb.cpp
    ${SOURCE_DIR}/src/Instability.cpp
    ${SOURCE_DIR}/src/Scale.cpp
    ${SOURCE_DIR}/src/Position.cpp
    ${SOURCE_DIR}/src/Rotate.cpp
    ${SOURCE_DIR}/src/Spin.cpp
    ${SOURCE_DIR}/src/Fold.cpp
    ${SOURCE_DIR}/src/Stretch.cpp
    ${SOURCE_DIR}/src/Shear.cpp
    ${SOURCE_DIR}/src/Smooth.cpp
    ${SOURCE_DIR}/src/Wiggle.cpp
    ${SOURCE_DIR}/src/Crest.cpp
    ${SOURCE_DIR}/src/Quake.cpp
    ${SOURCE_DIR}/src/Steps.cpp
    ${SOURCE_DIR}/src/Copycat.cpp
    ${SOURCE_DIR}/src/Petals.cpp
    ${SOURCE_DIR}/src/Knots.cpp
    ${SOURCE_DIR}/src/Vortex.cpp
    ${SOURCE_DIR}/src/Trough.cpp
    ${SOURCE_DIR}/src/Switch.cpp
    ${SOURCE_DIR}/src/Route.cpp
    ${SOURCE_DIR}/src/SumMult.cpp
    ${SOURCE_DIR}/src/SwitchXL.cpp
    ${SOURCE_DIR}/src/RouteXL.cpp
    ${SOURCE_DIR}/src/SumMixXL.cpp
    ${SOURCE_DIR}/src/Andxy.cpp
    ${SOURCE_DIR}/src/Orxy.cpp
    ${SOURCE_DIR}/src/Xorxy.cpp
    ${SOURCE_DIR}/src/ChopXL.cpp
    ${SOURCE_DIR}/src/Rescale.cpp
    ${SOURCE_DIR}/src/Bleed.cpp
    ${SOURCE_DIR}/src/Lisa.cpp
    ${SOURCE_DIR}/src/Rose.cpp
    ${SOURCE_DIR}/src/Eunice.cpp
    ${SOURCE_DIR}/src/SteppedSlew.cpp
    ${SOURCE_DIR}/src/Bitterroot.cpp
)

# ── Include paths ────────────────────────────────────────────────────
target_include_directories(Garmire PRIVATE
    ${SOURCE_DIR}/src
)

# ── Create plugin ────────────────────────────────────────────────────
create_plugin(
    SOURCE_LIB      Garmire
    PLUGIN_NAME     Garmire
    PLUGIN_JSON     ${SOURCE_DIR}/plugin.json
    SOURCE_ASSETS   ${CMAKE_CURRENT_LIST_DIR}/assets
    DESTINATION     ${CMAKE_CURRENT_LIST_DIR}/output
)
