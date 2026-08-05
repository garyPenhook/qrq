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
- [ ] Review and document the remaining differences from upstream.

## Phase 1: Correctness and safety

- [x] Replace fixed-size audio generation buffers with a capacity-checked buffer.
- Remove the audio-completion data race and define thread ownership clearly.
- Check allocation, audio, file, and thread errors consistently.
- Replace shell-based file copies and predictable temporary files.
- Make config and score updates atomic and migrate existing user data safely.
- Add focused tests for scoring, Morse timing, callbase parsing, and config I/O.

## Phase 2: Build and release engineering

- Add strict GCC and Clang warning builds plus ASan/UBSan jobs.
- Detect ncurses and audio dependencies with `pkg-config`.
- Add Linux, macOS, and MinGW CI.
- Separate install prefix from packaging `DESTDIR`.
- Add release archives and current Linux/Windows packaging.

## Phase 3: Training options

- Configurable session lengths instead of a fixed 50/all choice.
- Independent speed-up and speed-down steps.
- Accuracy-target and fixed-speed session policies.
- Missed-item review queue and error-weighted adaptive selection.
- Filters for call length, prefixes, digits, portable suffixes, and characters.
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
