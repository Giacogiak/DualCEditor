#pragma once

#include "field_glsl.h"

#include <QString>

// Exit code of a snapshot that could not get an OpenGL 3.3 core context; the
// ctest maps it to "skipped" (SKIP_RETURN_CODE).
inline constexpr int kExitNoGl = 77;

// Render one width x height frame of `scene` off-screen (QOffscreenSurface +
// FBO, no window) on the framed default view and write it to `path` as a PNG.
// Returns 0 on success, kExitNoGl without a GL 3.3 core context, 1 otherwise.
int renderSnapshot(const dce::fieldgraph::GlslScene& scene, const QString& path,
                   int width, int height);
