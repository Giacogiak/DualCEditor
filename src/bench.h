#pragma once

#include "field_glsl.h"

#include <QSize>
#include <QString>

// The measurement run behind `--bench` (docs/command_reference/01-viewport.md,
// scripts/measure_viewport.py). It shows a bare Viewport whose traced size is
// `size` device pixels and times two phases of `frames` frames each:
// - moving: a scripted orbit, one step per presented frame, so the view
//   previews at `previewScale` as under a held drag;
// - settled: full-resolution frames of a still camera, one per presented frame.
// Each phase records the GL_TIME_ELAPSED time of every frame and the wall time
// between presented frames (which counts Qt's composition and the swap too).
struct BenchOptions {
  QString graph;  // the path the result names
  float previewScale = 0.5f;
  QSize size{1280, 800};
  int frames = 60;
};

// Prints one JSON object on stdout: the medians of each phase, the compile ms,
// the wall ms from show() to the first presented frame, the sizes traced,
// GL_RENDERER and GL_VERSION. Returns 0, kExitNoGl without a GL 3.3 core
// context, 1 when the shader does not link or the run stalls.
int runBench(dce::fieldgraph::GlslScene scene, const BenchOptions& options);
