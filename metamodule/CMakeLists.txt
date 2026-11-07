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

# ── Source files (dynamically synchronized from Makefile) ────────────
set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SOURCE_DIR}/Makefile")

file(STRINGS "${SOURCE_DIR}/Makefile" MAKEFILE_SOURCES REGEX "^SOURCES \\+= src/.*\\.cpp$")
set(GARMIRE_SOURCES)
foreach(LINE ${MAKEFILE_SOURCES})
    string(REGEX REPLACE "^SOURCES \\+= src/(.*\\.cpp)$" "\\1" SRC_FILE "${LINE}")
    list(APPEND GARMIRE_SOURCES "${SOURCE_DIR}/src/${SRC_FILE}")
endforeach()

target_sources(Garmire PRIVATE ${GARMIRE_SOURCES})

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
