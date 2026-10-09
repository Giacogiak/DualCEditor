# P02 — Measured go/no-go

The spikes' numbers decide whether the full editor proceeds as planned, and in what form.
This phase writes no product code.

### T1. Write the go/no-go note

Compare P00's and P01's measurements with the targets in
`docs/roadmap/01-feasibility.md`: an interactive orbit at about 0.35–0.5 preview scale,
then a crisp settle. Write `docs/roadmap/03-go-no-go.md`, covering:
- what was met;
- what was missed, and by how much;
- which deferred items (E-21, E-22, E-23) the numbers trigger;
- the changes P03–P06 need.

Update `docs/decisions/03-deferred.md` for every trigger that fired.

#### Acceptance criteria
- `docs/roadmap/03-go-no-go.md` exists and is indexed. It cites rows of
  `02-measurements.md` rather than restating numbers from memory.
- Each fired trigger is recorded on its item.
- The handoff lists the plan edits the note proposes. They are not applied: the owner
  approves them at this phase's gate.

## Exit criteria
- T1 is met, and the owner has approved the note before P03 starts.
