# Command reference

The usage contract of `DualCEditor`: launch flags with their real defaults, keys and
mouse bindings, panels. A unit that adds or changes any of these updates its page here in
the same session.

Keys must work on non-QWERTY layouts (the owner uses Belgian AZERTY): use Qt key events
by meaning (`QKeyEvent::text()`, arrows, function keys), never by physical position.

## Files

- `01-viewport.md` — launch flags (`--snapshot`, `--preview-scale`), exit codes, the viewport's mouse and key bindings, and the paint log.
