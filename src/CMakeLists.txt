set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(SOURCE_DIR "${ROOT_DIR}/src")
file(GLOB_RECURSE SOURCE_FILES "${SOURCE_DIR}/*.cpp" "${SOURCE_DIR}/*.h" "${SOURCE_DIR}/*.hpp")   

# 对 SourceFile 变量里面的所有文件分类(保留资源管理器的目录结构)
foreach(fileItem ${SOURCE_FILES})       
    # Get the directory of the source file
    get_filename_component(PARENT_DIR "${fileItem}" DIRECTORY)
    # Remove common directory prefix to make the group
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
    # Make sure we are using windows slashes
    string(REPLACE "/" "\\" GROUP "${GROUP}")
    # Group into "Source Files" and "Header Files"
    set(GROUP "${GROUP}")
    source_group("${GROUP}" FILES "${fileItem}")
endforeach()

source_group(TREE "${ROOT_DIR}" FILES ${SOURCE_FILES})

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/Plugin.h")
configure_file(
   "${ROOT_DIR}/cmake/Plugin.h.in"
   "${VERSION_HEADER}"
   @ONLY
)

source_group("src" FILES "${VERSION_HEADER}")

configure_file(
   "${ROOT_DIR}/cmake/version.rc.in"
   "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
   @ONLY
)

add_library(
   "${PROJECT_NAME}"
   SHARED
   ${SOURCE_FILES}
   "${VERSION_HEADER}"
   "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
   "${ROOT_DIR}/.clang-format"
   "${ROOT_DIR}/.editorconfig"
)

target_compile_features(
   "${PROJECT_NAME}"
   PRIVATE
      cxx_std_20
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
   target_compile_options(
      "${PROJECT_NAME}"
      PRIVATE
      "/sdl"   # Enable Additional Security Checks
      "/utf-8"   # Set Source and Executable character sets to UTF-8
      "/Zi"   # Debug Information Format

      "/await"

      "/permissive-"   # Standards conformance
      "/Zc:preprocessor"   # Enable preprocessor conformance mode

      "/wd4200" # nonstandard extension used : zero-sized array in struct/union

      "$<$<CONFIG:DEBUG>:>"
      "$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
   )

   target_link_options(
      "${PROJECT_NAME}"
      PRIVATE
      "$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
      "$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
   )
endif()

target_include_directories(
   "${PROJECT_NAME}"
   PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}/src"
      "${SOURCE_DIR}"
)

target_compile_definitions(
   "${PROJECT_NAME}"
   PRIVATE
      WHEELER_EXPORTS
)

set(SKSE_SUPPORT_XBYAK ON)

add_subdirectory("$ENV{CommonLibSSEPath_NG}" CommonLibSSE EXCLUDE_FROM_ALL)

find_package(xbyak REQUIRED CONFIG)

find_package(imgui REQUIRED)

find_package(Freetype REQUIRED)



target_link_libraries(
   "${PROJECT_NAME}"
   PRIVATE
      CommonLibSSE::CommonLibSSE
      xbyak::xbyak
      imgui::imgui
      Freetype::Freetype
)

# Tracy profiler client, statically linked into the plugin. Off by default, so a
# normal build needs neither the submodule nor the client. Profiling.h turns the
# zone macros into nothing when it is off.
if(WHEELER_TRACY)
   set(TRACY_DIR "${ROOT_DIR}/extern/tracy")
   if(NOT EXISTS "${TRACY_DIR}/CMakeLists.txt")
      message(FATAL_ERROR "WHEELER_TRACY is ON but extern/tracy is empty. Run: git submodule update --init extern/tracy")
   endif()

   set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
   # Collect only while the Tracy profiler is connected, so a Tracy build can be
   # left installed without buffering every frame from game start.
   set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
   # Leave crashes to the game's crash logger. Tracy's handler would otherwise
   # take the exception first.
   set(TRACY_NO_CRASH_HANDLER ON CACHE BOOL "" FORCE)
   # Listen on this machine only. Discovery broadcasts then stay on loopback too,
   # so the profiler still finds the game without exposing it to the network.
   set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)

   add_subdirectory("${TRACY_DIR}" tracy EXCLUDE_FROM_ALL)

   # Other SKSE plugins (Huginn) carry their own Tracy client, and the first one
   # loaded takes the default port, 8086. A fixed port keeps Wheeler's client at
   # the same address whatever the load order: connect to 127.0.0.1:8087.
   # Tracy reads this when it builds the client, so it goes on TracyClient.
   target_compile_definitions(TracyClient PRIVATE TRACY_DATA_PORT=8087)

   target_link_libraries("${PROJECT_NAME}" PRIVATE Tracy::TracyClient)
   target_compile_definitions("${PROJECT_NAME}" PRIVATE WHEELER_TRACY)
endif()

target_precompile_headers(
   "${PROJECT_NAME}"
   PRIVATE
      "${SOURCE_DIR}/PCH.h"
)

install(
   FILES
      "$<TARGET_FILE:${PROJECT_NAME}>"
   DESTINATION "SKSE/Plugins"
   COMPONENT "main"
)

install(
   FILES
      "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
   DESTINATION "/"
   COMPONENT "pdbs"
)

if("${COPY_OUTPUT}")
   add_custom_command(
      TARGET "${PROJECT_NAME}"
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${CompiledPluginsPath}/SKSE/Plugins/"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${CompiledPluginsPath}/SKSE/Plugins/"
      VERBATIM
   )
endif()
