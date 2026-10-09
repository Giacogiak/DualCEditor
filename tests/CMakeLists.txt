# GL-free tests: they run on any CI runner, with no display.

# The editor's link to DualC: a field graph parses and compiles to a complete
# trace shader through the pinned DualC example libraries.
add_executable(codegen_smoke codegen_smoke.cpp)
target_link_libraries(codegen_smoke PRIVATE dualc_examples_fieldglsl)
dualceditor_target_options(codegen_smoke)
add_test(NAME codegen_smoke COMMAND codegen_smoke)
