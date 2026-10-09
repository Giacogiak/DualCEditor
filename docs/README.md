# DualCEditor Documentation

## Ownership by topic

| Folder | Owner | Content |
|--------|-------|---------|
| `design/` | Architecture | How the editor is structured: scene model, rendering pipeline, threading, undo/redo |
| `decisions/` | Design choices | Deferred, dropped, and settled decisions with rationale |
| `roadmap/` | Status tracking | Current phase, blockers, next-up work, measurements |
| `command_reference/` | UI contract | CLI (if any), UI panels, keyboard shortcuts, serialization |

## Quick links

- **Roadmap 00** (`roadmap/00-phase-0-spikes.md`): Spike A (QOpenGLWidget host) and Spike B (id-returning codegen).
- **Design** (`design/01-scene-model.md`): Multi-object scene, field graph integration, render path.
- **Decisions** (`decisions/01-settled.md`): Qt choice, separate repo, current lighting (no shadows).

## Reading order

Start here if you are:
- **Building or extending the editor:** Read design/01, then roadmap/00.
- **Debugging a render issue:** Read design/02-rendering.md, then look in src/.
- **Adding a UI feature:** Read command_reference/, then design/03-ui.md.
- **Understanding deferred work:** Read decisions/02-deferred.md.
