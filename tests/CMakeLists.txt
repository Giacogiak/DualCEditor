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

# `DualCEditor --bench` (the measurement run of scripts/measure_viewport.py)
# times both phases of a small view and prints one JSON line with GPU times.
add_test(NAME bench
  COMMAND DualCEditor --bench --bench-size 160x100 --bench-frames 4
          --preview-scale 0.5
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json)
set_tests_properties(bench PROPERTIES
  SKIP_RETURN_CODE 77
  PASS_REGULAR_EXPRESSION
  "\"moving\":{\"gpuFrames\":[1-9][0-9]*,\"gpuMs\":[0-9.]+,\"traced\":\\[80,50\\].*\"settled\":{\"gpuFrames\":[1-9][0-9]*,\"gpuMs\":[0-9.]+,\"traced\":\\[160,100\\]")

# The scene lowering, GL-free: ids in every function and uniform name, the
# object uniforms and their values, the union trace box, the colour patch.
add_executable(scene_test scene_test.cpp)
target_link_libraries(scene_test PRIVATE dualceditor_core)
dualceditor_target_options(scene_test)
add_test(NAME scene COMMAND scene_test)

# The undo history without GL: a sequence of commands undone to the empty
# scene and redone, the same scene and the same lowered shader at every step.
add_executable(scene_history_test scene_history_test.cpp)
target_link_libraries(scene_history_test PRIVATE dualceditor_core)
dualceditor_target_options(scene_history_test)
add_test(NAME scene_history COMMAND scene_history_test)

# The transform gizmo without GL (P03 T3): pose values, a move, a rotation
# and a scale drag, a pose as one undo step, and the trace and culling boxes
# following a move.
add_executable(gizmo_test gizmo_test.cpp)
target_link_libraries(gizmo_test PRIVATE dualceditor_core)
dualceditor_target_options(gizmo_test)
add_test(NAME gizmo COMMAND gizmo_test)

# A lowered scene on GL, off-screen: one object renders the graph's own image,
# three objects render in their own colours, and hiding, showing and
# recolouring compile and link nothing.
add_executable(scene_render_test scene_render_test.cpp)
target_link_libraries(scene_render_test PRIVATE dualceditor_core)
dualceditor_target_options(scene_render_test)
add_test(NAME scene_render
  COMMAND scene_render_test
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json)
set_tests_properties(scene_render PROPERTIES SKIP_RETURN_CODE 77)

# The Objects dock of a live MainWindow: two drops give objects in the panel
# and on screen; show and hide link nothing and restore the image; selecting a
# row selects the object; Delete removes it, down to an empty viewport.
add_executable(objects_panel_test objects_panel_test.cpp)
target_link_libraries(objects_panel_test PRIVATE dualceditor_core)
dualceditor_target_options(objects_panel_test)
add_test(NAME objects_panel COMMAND objects_panel_test)
set_tests_properties(objects_panel PROPERTIES SKIP_RETURN_CODE 77)

# The CPU pick (E-07), GL-free: two spheres overlapping on screen, the nearest
# along the ray wins, a hidden one is skipped, empty space picks nothing; a
# transform edit and a TPMS object; viewRay is the shader's ray.
add_executable(pick_test pick_test.cpp)
target_link_libraries(pick_test PRIVATE dualceditor_core)
dualceditor_target_options(pick_test)
add_test(NAME pick COMMAND pick_test)

# The asynchronous compile (P07 T2, E-21): a build swaps the program while
# frames are drawn with no frame mixing the two scenes, the last of two builds
# wins, a failure keeps the image, and in the window an add during a compile
# gives the last edit's scene.
add_executable(async_compile_test async_compile_test.cpp)
target_link_libraries(async_compile_test PRIVATE dualceditor_core)
dualceditor_target_options(async_compile_test)
add_test(NAME async_compile
  COMMAND async_compile_test
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json)
set_tests_properties(async_compile PROPERTIES SKIP_RETURN_CODE 77)

# The .dcscene file (P03 T2, E-08), GL-free: a save then load gives the same
# objects, field values at sample points, shader, uniform values and mesh
# bytes; moved to another folder with its sources gone it still loads; a
# future version, no version or no zip is refused with a message.
add_executable(scene_file_test scene_file_test.cpp)
target_link_libraries(scene_file_test PRIVATE dualceditor_core)
dualceditor_target_options(scene_file_test)
add_test(NAME scene_file
  COMMAND scene_file_test
          ${DUALCEDITOR_DUALC_SOURCE_DIR}/examples/samples/gyroid_box.json)

# The mesh readers (P04 T1, E-09), GL-free: one fixture per format (OBJ, PLY,
# STL) gives its vertex and face counts after welding; malformed files give an
# error message, not a crash; a mesh registered by id lowers as mesh(id=...).
add_executable(mesh_import_test mesh_import_test.cpp)
target_link_libraries(mesh_import_test PRIVATE dualceditor_core)
dualceditor_target_options(mesh_import_test)
add_test(NAME mesh_import
  COMMAND mesh_import_test ${CMAKE_CURRENT_SOURCE_DIR}/data/meshes)
