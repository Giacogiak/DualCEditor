# DualCEditor documentation

Every folder has a `README.md` naming every file in it; `scripts/check.py` enforces it.

## Who owns which fact

| Folder | Owns |
|---|---|
| `design/` | How the editor is built today: scene model, render path, threading, file formats. Written as the code lands. |
| `decisions/` | Settled choices (not re-litigated), open decisions with the phases they block, deferred work with its trigger. |
| `roadmap/` | Status snapshot, the feasibility analysis, measurements, the go/no-go, the record of the phased plan that built the editor (`roadmap/05-plan-record/`, one file per phase), the questions owed to DualC. |
| `command_reference/` | The usage contract: launch flags, keys and mouse, panels. Updated in the same session as the UI. |

## Files

- `design/` — architecture, see its README.
- `decisions/` — see its README.
- `roadmap/` — see its README.
- `command_reference/` — see its README.
