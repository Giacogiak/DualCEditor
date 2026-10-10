# GL-free tests run on any CI runner, with no display. Tests that need GL
# report skipped (exit 77) when no OpenGL 3.3 core context can be made.

# The editor's link to DualC: a field graph parses and compiles to a complete
# trace shader through the pinned DualC example libraries.
add_executable(codegen_smoke codegen_smoke.cpp)
target_link_libraries(codegen_smoke PRIVATE dualc_examples_fieldglsl)
dualceditor_target_options(codegen_smoke)
add_test(NAME codegen_smoke COMMAND codegen_smoke)

# `DualCEditor --snapshot` renders the default graph off-screen to a PNG that
# shows geometry, not a flat colour or the bare background.
add_executable(snapshot_test snapshot_test.cpp)
target_link_libraries(snapshot_test PRIVATE Qt6::Core Qt6::Gui)
dualceditor_target_options(snapshot_test)
add_test(NAME snapshot
  COMMAND snapshot_test $<TARGET_FILE:DualCEditor>
          ${CMAKE_CURRENT_BINARY_DIR}/snapshot.png)
set_tests_properties(snapshot PROPERTIES SKIP_RETURN_CODE 77)

# The orbit camera's drag, dolly, pan and frame maths. GL-free.
add_executable(orbit_camera_test orbit_camera_test.cpp)
target_link_libraries(orbit_camera_test PRIVATE dualceditor_core)
dualceditor_target_options(orbit_camera_test)
add_test(NAME orbit_camera COMMAND orbit_camera_test)

# A live Viewport under synthetic input: preview while moving, one full frame
# after the settle time, no paintGL while idle, F by meaning. Needs a display.
add_executable(viewport_test viewport_test.cpp)
target_link_libraries(viewport_test PRIVATE dualceditor_core)
dualceditor_target_options(viewport_test)
add_test(NAME viewport
  COMMAND viewport_test
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json 0.25)
set_tests_properties(viewport PROPERTIES SKIP_RETURN_CODE 77)
# The same with the preview off: full-resolution frames only, and no extra
# settle repaint.
add_test(NAME viewport_no_preview
  COMMAND viewport_test
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json 1.0)
set_tests_properties(viewport_no_preview PROPERTIES SKIP_RETURN_CODE 77)

# A live MainWindow: a drop of a .json and a .fld renders it, the file watch
# reloads a save and keeps the camera, a broken file or shader shows its error
# and keeps the image; the command-line argument through --snapshot.
add_executable(main_window_test main_window_test.cpp)
target_link_libraries(main_window_test PRIVATE dualceditor_core)
dualceditor_target_options(main_window_test)
add_test(NAME main_window
  COMMAND main_window_test $<TARGET_FILE:DualCEditor>
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json)
set_tests_properties(main_window PROPERTIES SKIP_RETURN_CODE 77)

# The Parameters dock: an edit of the gyroid wavelength or the onion thickness
# changes the image and links no program; the status bar shows the compile ms
# and the GPU ms of the last settled frame; a reload rebuilds the dock.
add_executable(parameter_dock_test parameter_dock_test.cpp)
target_link_libraries(parameter_dock_test PRIVATE dualceditor_core)
dualceditor_target_options(parameter_dock_test)
add_test(NAME parameter_dock
  COMMAND parameter_dock_test
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json)
set_tests_properties(parameter_dock PROPERTIES SKIP_RETURN_CODE 77)
