# Fork delta from upstream

This fork tracks the content of upstream QRQ commit
`6174eeebec97cf56a041663cd1558beddee75b36` (QRQ 0.3.5 plus its maintenance
commits). The Git histories are unrelated, so this is a content comparison,
not a merge-base comparison.

The fork-specific changes are deliberately limited to these files:

- `.github/workflows/ci.yml` adds GitHub Actions builds, static analysis, and
  sanitizer smoke coverage.
- `ROADMAP.md` and `TODO.md` record the modernization plan and its status.
- `src/Makefile` preserves caller-provided warning/sanitizer flags while
  retaining QRQ's required backend flags.
- `src/pulseaudio.c` hardens dynamic audio buffering and error handling.
- `src/qrq.c` contains the safety, persistence, training-option, and strict-C
  changes described below.
- `src/qrqrc` documents the fork's new session-length and independent-speed
  settings.

## Behavioral additions

- Configurable session length (`sessionlength`, default 50) and independent
  correct/error speed steps (`speedupstep` / `speeddownstep`).
- Atomic configuration and toplist rewrites.
- Dynamic audio and attempt-summary buffers.
- Safe startup file copying and direct gnuplot input streaming.

## Compatibility commitments

- Existing `speedstep` remains accepted and initializes both newer speed-step
  settings.
- Existing user config, toplist, callbase, and summary locations are retained.
- Standard 50-call sessions remain the comparable high-score mode.

## Maintenance policy

When importing a later upstream release, compare its content against this
document first, preserve the compatibility commitments above, and add only
fork-specific changes that are still necessary.
