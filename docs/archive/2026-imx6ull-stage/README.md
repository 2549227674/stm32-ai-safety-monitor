# Archive: 2026 i.MX6ULL Stage

This directory contains documentation and task files from the i.MX6ULL stage of the project.

## Historical Context

The project originally planned a dual-board architecture: i.MX6ULL Pro for local safety control + Orange Pi 5 for AI inference. Due to i.MX6ULL late-stage power/boot issues, the demo mainline was migrated to OPi5 single-board dual-process.

## What's Here

- `tasks/` — i.MX6ULL-era task files (Task02, 03, 04, 07, 08, 10, 11)
- `docs/` — i.MX6ULL-era design documents
- `hardware/` — i.MX6ULL pinmap and wiring references
- `VERTICAL_SLICE_INTEGRATION_CHECKLIST.md` — original end-to-edge checklist

## Key Evidence

All test evidence for the i.MX6ULL stage is preserved in `tests/imx6ull/` and `tests/integration/` in the main repository.

## Current Mainline

See `CANONICAL_DECISIONS.md` and `CLAUDE_CODE_TASK_DOCS_OPI5_ALIGNMENT.md` for the current OPi5 demo mainline.
