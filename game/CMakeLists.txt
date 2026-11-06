dreamcomp_add_port(armymensh
  TITLE "Army Men: Sarge's Heroes"
  OUTPUT_NAME army-men-sarges-heroes-recomp
  SOURCES ${PROJECT_SOURCE_DIR}/src/armymensh.cpp
          ${PROJECT_SOURCE_DIR}/src/disc_variant.cpp
          ${PROJECT_SOURCE_DIR}/src/mouse_camera.cpp
          ${PROJECT_SOURCE_DIR}/src/widescreen.cpp
          ${PROJECT_SOURCE_DIR}/src/startup_logos.cpp
          ${PROJECT_SOURCE_DIR}/src/snow_intro_audio.cpp)

# Experimental PAL targets use separately mapped executable addresses.
option(ARMYMENSH_PAL_SUPPORT "Build the PAL English60 preview runtime" OFF)
option(ARMYMENSH_PAL_PROBE "Build isolated PAL development tools" OFF)
option(ARMYMENSH_PAL_OPTIMIZE "Optimize PAL guest translations (disable for faster iteration)" ON)
if(ARMYMENSH_PAL_PROBE OR ARMYMENSH_PAL_SUPPORT)
  add_executable(armymensh_pal_extract EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/prepare_pal.cpp)
  target_link_libraries(armymensh_pal_extract PRIVATE dream::runtime)
  dream_target_defaults(armymensh_pal_extract)
  if(ARMYMENSH_PAL_PROBE AND DREAM_DEV_INTERPRETER)
  add_executable(armymensh_pal_probe EXCLUDE_FROM_ALL
    ${DREAMCOMP_ROOT}/engine/runtime/boot/boot_main.cpp
    ${PROJECT_SOURCE_DIR}/tools/pal_probe.cpp
    ${PROJECT_SOURCE_DIR}/tools/pal_diagnostics.cpp)
  target_link_libraries(armymensh_pal_probe PRIVATE dream::runtime dream::translator)
  if(TARGET dream::render_vk)
    target_link_libraries(armymensh_pal_probe PRIVATE dream::render_vk)
    target_compile_definitions(armymensh_pal_probe PRIVATE DREAM_WITH_RENDERER)
  endif()
  if(TARGET dream::audio)
    target_link_libraries(armymensh_pal_probe PRIVATE dream::audio)
    target_compile_definitions(armymensh_pal_probe PRIVATE DREAM_WITH_AUDIO)
  endif()
  dream_target_defaults(armymensh_pal_probe)
  endif()

  # Both executables replace the same RAM addresses. Require instruction signatures,
  # and read literal pools at runtime while the stock loader performs the handoff.
  set(_pal_units)
  foreach(_profile armymensh_pal_probe armymensh_pal_en60 armymensh_pal_xfer)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extracted/pal/1ST_READ.BIN" AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extracted/pal/KLINKENGLISH60HZ.BIN" AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extracted/pal/GINXFER.CODE.BIN")
      set(_out "${CMAKE_CURRENT_BINARY_DIR}/pal-gen/${_profile}.cpp")
      set(_profile_units "${_out}")
      file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${_profile}.toml" _pal_config)
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_profile}.toml")
      string(REGEX MATCHALL "dest = 0x[0-9A-Fa-f]+" _pal_dests "${_pal_config}")
      foreach(_dest IN LISTS _pal_dests)
        string(REGEX REPLACE "dest = 0x" "" _dest "${_dest}")
        string(TOLOWER "${_dest}" _dest)
        list(APPEND _profile_units "${CMAKE_CURRENT_BINARY_DIR}/pal-gen/${_profile}_reloc_${_dest}.cpp")
      endforeach()
      add_custom_command(OUTPUT ${_profile_units}
        COMMAND dream-translate game --config "${CMAKE_CURRENT_SOURCE_DIR}/${_profile}.toml"
                --out-dir "${CMAKE_CURRENT_BINARY_DIR}/pal-gen" --overlay-full --no-fold
        DEPENDS dream-translate "${CMAKE_CURRENT_SOURCE_DIR}/${_profile}.toml"
                "${CMAKE_CURRENT_SOURCE_DIR}/extracted/pal/1ST_READ.BIN"
                "${CMAKE_CURRENT_SOURCE_DIR}/extracted/pal/KLINKENGLISH60HZ.BIN"
                "${CMAKE_CURRENT_SOURCE_DIR}/extracted/pal/GINXFER.CODE.BIN"
        VERBATIM)
      list(APPEND _pal_units ${_profile_units})
    endif()
  endforeach()
  if(_pal_units)
    # Share the large translations between stock and enhanced experiments.
    add_library(armymensh_pal_guest OBJECT EXCLUDE_FROM_ALL ${_pal_units})
    target_link_libraries(armymensh_pal_guest PRIVATE dream::runtime dream::translator)
    dream_target_defaults(armymensh_pal_guest)
    foreach(_target armymensh_pal_native armymensh_pal_controls)
      add_executable(${_target} EXCLUDE_FROM_ALL
        ${DREAMCOMP_ROOT}/engine/runtime/boot/boot_main.cpp
        ${PROJECT_SOURCE_DIR}/tools/pal_diagnostics.cpp)
      target_link_libraries(${_target} PRIVATE armymensh_pal_guest dream::runtime dream::translator)
      if(TARGET dream::render_vk)
        target_link_libraries(${_target} PRIVATE dream::render_vk)
        target_compile_definitions(${_target} PRIVATE DREAM_WITH_RENDERER)
      endif()
      if(TARGET dream::audio)
        target_link_libraries(${_target} PRIVATE dream::audio)
        target_compile_definitions(${_target} PRIVATE DREAM_WITH_AUDIO)
      endif()
      dream_target_defaults(${_target})
    endforeach()
    target_sources(armymensh_pal_native PRIVATE ${PROJECT_SOURCE_DIR}/tools/pal_stock_hooks.cpp)
    target_sources(armymensh_pal_controls PRIVATE
      ${PROJECT_SOURCE_DIR}/src/pal_boot.cpp
      ${PROJECT_SOURCE_DIR}/src/startup_logos.cpp
      ${PROJECT_SOURCE_DIR}/src/pal_port.cpp
      ${PROJECT_SOURCE_DIR}/src/mouse_camera.cpp
      ${PROJECT_SOURCE_DIR}/src/widescreen.cpp)
    target_sources(armymensh_pal_controls PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/armymensh_port_version.cpp")
    target_link_libraries(armymensh_pal_controls PRIVATE dreamcomp_core)
    target_compile_definitions(armymensh_pal_controls PRIVATE ARMY_PAL_EN60
      DREAMCOMP_PORT_ID="armymensh_pal_en60" DREAMCOMP_PORT_TITLE="Army Men PAL English60 preview")
    if(TARGET dream::render_vk)
      target_sources(armymensh_pal_controls PRIVATE ${PROJECT_SOURCE_DIR}/src/native_menus.cpp)
      add_executable(armymensh_pal_menu_tests EXCLUDE_FROM_ALL
        ${PROJECT_SOURCE_DIR}/tools/test_native_menus.cpp ${PROJECT_SOURCE_DIR}/src/native_menus.cpp)
      target_link_libraries(armymensh_pal_menu_tests PRIVATE dream::render_vk)
      target_compile_definitions(armymensh_pal_menu_tests PRIVATE ARMY_PAL_EN60)
    endif()
    if(NOT MSVC)
      if(ARMYMENSH_PAL_OPTIMIZE)
        set_source_files_properties(${_pal_units} PROPERTIES COMPILE_OPTIONS "-O1")
      else()
        set_source_files_properties(${_pal_units} PROPERTIES COMPILE_OPTIONS "-O0")
      endif()
    endif()
  endif()
endif()

if(TARGET dream::render_vk)
  if(TARGET armymensh_boot)
    target_sources(armymensh_boot PRIVATE ${PROJECT_SOURCE_DIR}/src/native_menus.cpp)
  endif()
  add_executable(armymensh_menu_tests EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/test_native_menus.cpp
                 ${PROJECT_SOURCE_DIR}/src/native_menus.cpp)
  set_target_properties(armymensh_menu_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tools")
  target_link_libraries(armymensh_menu_tests PRIVATE dream::render_vk)
  add_executable(armymensh_render_capture EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/render_capture.cpp)
  set_target_properties(armymensh_render_capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tools")
  target_sources(armymensh_render_capture PRIVATE ${DREAMCOMP_ROOT}/src/hud.cpp)
  target_include_directories(armymensh_render_capture PRIVATE ${DREAMCOMP_ROOT}/include)
  target_link_libraries(armymensh_render_capture PRIVATE dream::render_vk)
  target_compile_features(armymensh_render_capture PRIVATE cxx_std_20)
endif()

if(TARGET dream::render)
  add_executable(armymensh_interpolation_tests EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/test_interpolation.cpp)
  set_target_properties(armymensh_interpolation_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tools")
  target_link_libraries(armymensh_interpolation_tests PRIVATE dream::render)
endif()

if(TARGET dream::render_vk)
  add_executable(armymensh_controls_tests EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/test_controller_controls.cpp)
  set_target_properties(armymensh_controls_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tools")
  target_link_libraries(armymensh_controls_tests PRIVATE dream::render_vk)
endif()


if(TARGET dream::render)
  add_executable(armymensh_transparency_tests EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/test_translucent_sort.cpp)
  set_target_properties(armymensh_transparency_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tools")
  target_link_libraries(armymensh_transparency_tests PRIVATE dream::render)
endif()

add_executable(armymensh_disc_tests EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/test_cdi.cpp)
set_target_properties(armymensh_disc_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tools")
target_link_libraries(armymensh_disc_tests PRIVATE dream::runtime)
target_compile_features(armymensh_disc_tests PRIVATE cxx_std_20)
target_sources(armymensh_disc_tests PRIVATE
  ${DREAMCOMP_ROOT}/src/frontend/launcher_model.cpp
  ${DREAMCOMP_ROOT}/src/settings.cpp)
target_include_directories(armymensh_disc_tests PRIVATE
  ${DREAMCOMP_ROOT}/include ${DREAMCOMP_ROOT}/src/frontend)

add_executable(armymensh_disc_revision_tests EXCLUDE_FROM_ALL
  ${PROJECT_SOURCE_DIR}/tools/test_disc_revisions.cpp
  ${PROJECT_SOURCE_DIR}/src/snow_intro_audio.cpp
  ${DREAMCOMP_ROOT}/src/frontend/launcher_model.cpp
  ${DREAMCOMP_ROOT}/src/settings.cpp)
set_target_properties(armymensh_disc_revision_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tools")
target_include_directories(armymensh_disc_revision_tests PRIVATE
  ${DREAMCOMP_ROOT}/include ${DREAMCOMP_ROOT}/src/frontend)
target_link_libraries(armymensh_disc_revision_tests PRIVATE dream::translator dream::runtime)
target_compile_features(armymensh_disc_revision_tests PRIVATE cxx_std_20)

add_executable(armymensh_widescreen_tests EXCLUDE_FROM_ALL
  ${PROJECT_SOURCE_DIR}/tools/test_widescreen.cpp ${PROJECT_SOURCE_DIR}/src/widescreen.cpp)
target_link_libraries(armymensh_widescreen_tests PRIVATE dream::runtime)
target_include_directories(armymensh_widescreen_tests PRIVATE ${DREAMCOMP_ROOT}/include)
target_compile_features(armymensh_widescreen_tests PRIVATE cxx_std_20)

foreach(_region usa pal)
  add_executable(armymensh_player_controls_${_region}_tests EXCLUDE_FROM_ALL
    ${PROJECT_SOURCE_DIR}/tools/test_player_controls.cpp
    ${PROJECT_SOURCE_DIR}/src/mouse_camera.cpp ${PROJECT_SOURCE_DIR}/src/widescreen.cpp
    ${DREAMCOMP_ROOT}/src/settings.cpp)
  target_link_libraries(armymensh_player_controls_${_region}_tests PRIVATE dream::runtime dream::render)
  target_include_directories(armymensh_player_controls_${_region}_tests PRIVATE ${DREAMCOMP_ROOT}/include)
  if(_region STREQUAL "pal")
    target_compile_definitions(armymensh_player_controls_${_region}_tests PRIVATE ARMY_PAL_EN60)
  endif()
endforeach()
if(TARGET dreamcomp_frontend)
  add_executable(armymensh_controller_profiles_ui_tests EXCLUDE_FROM_ALL
    ${PROJECT_SOURCE_DIR}/tools/test_controller_profiles_ui.cpp ${DREAMCOMP_ROOT}/src/settings.cpp)
  target_include_directories(armymensh_controller_profiles_ui_tests PRIVATE ${DREAMCOMP_ROOT}/src/frontend)
  target_link_libraries(armymensh_controller_profiles_ui_tests PRIVATE dreamcomp_frontend RmlUi::RmlUi)
endif()
