dreamcomp_add_port(armymensh
  TITLE "Army Men: Sarge's Heroes"
  OUTPUT_NAME army-men-sarges-heroes-recomp
  SOURCES ${PROJECT_SOURCE_DIR}/src/armymensh.cpp
          ${PROJECT_SOURCE_DIR}/src/mouse_camera.cpp
          ${PROJECT_SOURCE_DIR}/src/widescreen.cpp
          ${PROJECT_SOURCE_DIR}/src/startup_logos.cpp
          ${PROJECT_SOURCE_DIR}/src/snow_intro_audio.cpp)

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
