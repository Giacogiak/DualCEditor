# Semantic lint runs

The runs of the owner's `/docs-semantic-lint`, which checks the meaning of the docs: whether
`../../design/` still describes the code, whether the record and the usage pages link the
design layer instead of restating it, and whether a deferred trigger has come due. Each run
is one dated file. A later session changes only its disposition column, as a dated append
at the end of the file.

## Files

- `01-2026-10-14-first-run.md` — the first run, after P00–P07 and the owner's feedback: 21 claims contradicted by code, 11 design-page lags (pages 02 and 03), 1 orphan page.
