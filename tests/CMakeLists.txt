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
