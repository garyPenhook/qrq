# QRQ modernization roadmap

This fork aims to preserve QRQ's fast, keyboard-driven terminal experience
while making it safer, easier to build, and more useful for deliberate CW
practice.

## Phase 0: Upstream baseline

- [x] Attach the local tree to `garyPenhook/qrq`.
- [x] Import the current official upstream tree and post-0.3.5 maintenance.
- [x] Restore correct Backspace and Delete behavior.
- [x] Persist both `speedstep` and `stoponerror` configuration values.
- [x] Correct the maximum-length input boundary.
- [x] Review and document the remaining differences from upstream in
  `UPSTREAM_DIFF.md`.

## Phase 1: Correctness and safety

- [x] Replace fixed-size audio generation buffers with a capacity-checked buffer.
- [x] Remove the audio-completion data race and define thread ownership clearly.
- [x] Check allocation, audio, file, and thread errors consistently.
- [x] Replace shell-based file copies and predictable temporary files.
- [x] Make config and score updates atomic and migrate existing user data safely.
- [~] Add focused tests for scoring, Morse timing, callbase parsing, and config I/O.
  Scoring and callbase parsing are covered; timing and config I/O remain.

## Phase 2: Build and release engineering

- [x] Add GCC and Clang builds plus focused cppcheck analysis in GitHub Actions.
- [x] Add ASan/UBSan build and command-line smoke-test coverage.
- [x] Add a warning-free strict C17 syntax check.
- [x] Detect ncurses and audio dependencies with `pkg-config`, retaining
  linker fallbacks for older environments.
- [x] Add Linux, macOS, and MinGW CI.
- [x] Separate install prefix from packaging `DESTDIR`.
- [~] Add release archives and current Linux/Windows packaging. A source
  archive target remains; distributable platform packages need release policy.

## Phase 3: Training options

- [x] Configurable session lengths instead of a fixed 50/all choice.
- [x] Independent speed-up and speed-down steps.
- [x] Accuracy-target and fixed-speed session policies. Fixed speed is
  retained; `accuracytarget` can require 80–100% accuracy for a local
  toplist entry.
- [x] Add a missed-item review queue and error-weighted adaptive selection.
  Both modes are optional per session and excluded from the comparable toplist.
- [~] Filters for call length, prefixes, digits, portable suffixes, and
  characters. Length filtering is available through `qrqrc`.
- Seeded sessions for repeatable challenges.

## Phase 4: Feedback and statistics

- Record response time and per-item accuracy in a versioned history format.
- Add in-program score, speed, and accuracy trends without requiring gnuplot.
- Add character-confusion and frequently-missed-item views.
- Support history export and import.

## Phase 5: Advanced practice and UX

- Configurable pitch range, volume, spacing, and independent rise/fall times.
- Optional QRM, QRN, fading, and pileup simulation.
- Terminal resize handling, configurable keys, and accessible color themes.
- Headless practice generation and WAV export.
