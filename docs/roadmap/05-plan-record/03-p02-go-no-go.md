# P02 — Measured go/no-go

**DONE (2026-10-13).** The spikes' numbers decided whether the full editor proceeds, and
in what form. No product code. The note is `../03-go-no-go.md`; its verdict is **go**,
approved by the owner on 2026-10-13.

## T1. Write the go/no-go note

*Planned:* compare P00's and P01's measurements with `../01-feasibility.md`'s targets (an
interactive orbit at about 0.35–0.5, then a crisp settle); write `../03-go-no-go.md` with
what was met, what was missed and by how much, which deferred items (E-21, E-22, E-23)
the numbers trigger, and the changes P03–P06 need; update `../../decisions/03-deferred.md` for every
trigger that fired. Acceptance: the note exists, is indexed and cites rows of
`../02-measurements.md` rather than restating numbers; each fired trigger is recorded on its
item; the handoff lists the plan edits the note proposes, not applied: the owner approves
them at the phase gate.

**DONE (written 2026-10-12, commit b0dd8da; approved and applied 2026-10-13, commit
8b9baf8).** Delivered: `../03-go-no-go.md`, citing each number as *unit · scene · preview
scale* of the first table or by section. `../../decisions/03-deferred.md`: E-21 **Triggered (P02.1)** by its
second clause ("any phase from P03 onward"), E-23 **Planned (P02.1)**, E-22 **Not fired**
(no several-mesh scene measured). The feasibility note gives no number for "interactive";
the note reads it as at most 33 ms per moving frame.
The first session returned blocked on P02's gate (approval). **The owner's answers
(Giacomo, 2026-10-13), all applied in the second session:**
1. Go: approved. P03 may start once E-08 is decided.
2. 33 ms stays the interactive bar.
3. E-21 triggered now (*Confirmed* line on E-21, planned as P07 T2).
4. Option 1a: a new phase P07 (`04-p07-render-scaling.md`), run right after P02 since
   neither task depends on E-08: T1 culling (E-23), T2 asynchronous compile (E-21, no
   editor program cache), T3 the GTX re-measurement. Units P07.1–P07.3 were added to the
   runner's manifest on the owner's explicit instruction (the manifest was otherwise the
   owner's); gate `auto`, P07.3 the phase end, the same verification list as the others.
5. Items 2–5 of the note: P03 T3's acceptance gains "the trace and culling boxes follow a
   move as uniforms; a moved object is still drawn whole, with no compile"; P04 T2 gains a
   GTX check of 1, 3, 5 imported meshes through a `--meshes` family (E-22's trigger);
   P05 T2 gains a GTX check of the octet fill at 0.35 against P00.5 · tapered octet (P12)
   · 0.35 (E-28's trigger); E-28, an interruptible settle, is written into
   `../../decisions/03-deferred.md`. P06: no change.

The numbers the verdict rests on (all from `../02-measurements.md`): gyroid and
mesh_lattice met (moving 0.8–2.7 GPU ms at 0.35–0.5, settled 2.6–7.3 ms); 10 primitives and
3 TPMS-in-box met (settled 3.2 and 7.6 GPU ms); the tapered octet missed (80.6 GPU ms
moving at 0.35, 2.5× the bar; 385 ms settled), and `dualc_field_view` takes 73.0 ms on it,
so the cost is DualC's trace; cold compiles 157–1210 ms on the GUI thread, warm 14–31 ms;
growth +0.28 and +2.49 ms per added object. Nothing was measured in this unit.
- *Wrong on the way:* the new file's line was first appended out of order in the roadmap
  README's file list; moved. Nothing else.
- *Open then:* the start-of-drag lag on the octet is attributed to the in-flight settle
  frame from a reading of the render path, not measured (a bench phase starting a drag
  right after a settle would confirm it; E-28's trigger, P05 T2's GTX check, is the
  practical test). The status bar's settled `frame` reading high (P00 T5) stays open.

## Exit criteria

T1 met; the owner approved the note before P03 started (P07 ran first);
`python scripts/check.py` green (12/12 ctests).

---
← Back to the [Plan record](README.md) · [Roadmap index](../README.md).
