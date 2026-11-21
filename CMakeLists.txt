cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SPARK_PLUGINS_VERSION_LINE
     LIMIT_COUNT 1)
string(STRIP "${SPARK_PLUGINS_VERSION_LINE}" SPARK_PLUGINS_VERSION_LINE)
if(NOT SPARK_PLUGINS_VERSION_LINE MATCHES "^([0-9]+\.[0-9]+\.[0-9]+)$")
  message(FATAL_ERROR "VERSION must be a SemVer, e.g. 0.1.0")
endif()
project(spark-client-plugins VERSION ${SPARK_PLUGINS_VERSION_LINE} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  set(SPARK_PLUGINS_WARNING_FLAGS /W4 /permissive- /utf-8)
else()
  set(SPARK_PLUGINS_WARNING_FLAGS -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)

option(SPARK_PLUGINS_BUILD_TESTS "Build the plugin tests" ON)

# ---------------------------------------------------------------------------
# Plugin SDK
#
# The plugins only use the stable C ABI and SDK adapter shipped by the client.
# Point this at an installed sparkapp-core (/usr/include/spark-client) or at a
# spark-client source checkout (its top-level src/ directory). Nothing here
# includes or links the host implementation.
# ---------------------------------------------------------------------------
set(SPARK_PLUGIN_SDK_INCLUDE_DIR "" CACHE PATH
    "Directory containing plugin/api.h and plugin/sdk.h")
if(NOT SPARK_PLUGIN_SDK_INCLUDE_DIR)
  find_path(SPARK_PLUGIN_SDK_INCLUDE_DIR
    NAMES plugin/api.h
    HINTS
      "${CMAKE_INSTALL_PREFIX}/include/spark-client"
      /usr/local/include/spark-client
      /usr/include/spark-client)
endif()
if(NOT EXISTS "${SPARK_PLUGIN_SDK_INCLUDE_DIR}/plugin/api.h")
  message(FATAL_ERROR
    "Plugin SDK headers not found. Install sparkapp-core or pass "
    "-DSPARK_PLUGIN_SDK_INCLUDE_DIR=<spark-client>/src")
endif()
message(STATUS "Spark plugin SDK: ${SPARK_PLUGIN_SDK_INCLUDE_DIR}")

# Vendored nlohmann/json, used by the plugins only.
add_library(spark_plugin_json INTERFACE)
target_include_directories(spark_plugin_json INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# ---------------------------------------------------------------------------
# Plugins
# ---------------------------------------------------------------------------
function(spark_add_plugin target source)
  add_library(${target} MODULE "${source}")
  target_include_directories(${target} PRIVATE
    "${SPARK_PLUGIN_SDK_INCLUDE_DIR}")
  target_link_libraries(${target} PRIVATE spark_plugin_json)
  target_compile_options(${target} PRIVATE ${SPARK_PLUGINS_WARNING_FLAGS})
  # Hyphenated names match the host binaries (spark-client / spark-tui /
  # spark-gui) and the plugin ids.
  set_target_properties(${target} PROPERTIES PREFIX "")
  install(TARGETS ${target}
          LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/spark-client/plugins")
endfunction()

spark_add_plugin(spark-manage-news plugins/manage-news/manage_news.cpp)
spark_add_plugin(spark-manage-forum plugins/manage-forum/manage_forum.cpp)

if(SPARK_PLUGINS_BUILD_TESTS AND UNIX)
  enable_testing()
  add_executable(spark-plugin-tests tests/test_plugins.cpp)
  target_include_directories(spark-plugin-tests PRIVATE
    "${SPARK_PLUGIN_SDK_INCLUDE_DIR}")
  target_link_libraries(spark-plugin-tests PRIVATE
    spark_plugin_json
    ${CMAKE_DL_LIBS})
  target_compile_options(spark-plugin-tests PRIVATE
    ${SPARK_PLUGINS_WARNING_FLAGS})
  add_test(NAME spark-plugin-tests COMMAND spark-plugin-tests
           "$<TARGET_FILE:spark-manage-news>"
           "$<TARGET_FILE:spark-manage-forum>")
endif()
