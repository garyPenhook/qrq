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
  Scoring, callbase parsing, config parsing, and comment/line-ending-preserving
  config updates are covered; Morse timing and full UI round trips remain.

## Phase 2: Build and release engineering

- [x] Add GCC and Clang builds plus focused cppcheck analysis in GitHub Actions.
- [x] Add ASan/UBSan build and command-line smoke-test coverage.
- [x] Add a warning-free strict C17 syntax check.
- [x] Detect ncurses and audio dependencies with `pkg-config`, retaining
  linker fallbacks for older environments.
- [x] Add Linux, macOS, and MinGW CI.
- [x] Separate install prefix from packaging `DESTDIR`.
- [~] Add release archives and current Linux/Windows packaging. The source
  archive is now self-testing after extraction; platform packages still need
  release policy and signing.

## Phase 3: Training options

- [x] Configurable session lengths instead of a fixed 50/all choice.
- [x] Independent speed-up and speed-down steps.
- [x] Accuracy-target and fixed-speed session policies. Fixed speed is
  retained; `accuracytarget` can require 80–100% accuracy for a local
  toplist entry.
- [x] Add a missed-item review queue and error-weighted adaptive selection.
  Both modes are optional per session and excluded from the comparable toplist.
- [x] Filters for call length, prefixes, digits, portable suffixes, and
  characters through `qrqrc`; all constraints are applied by the loader.
- [x] Seeded sessions for repeatable challenges via `sessionseed` in `qrqrc`.
- [x] Add zero-padded three-, four-, and five-digit serial-exchange practice.
  The F5 call-filter page can replace the selected database with the complete
  serial range for a training-only contest-copy session.
- [x] Add focused portable-call variants. The W4GNS generator replaces base
  calls with /P, /M, and /MM forms for a training-only suffix-copy session.
- [x] Add sustained-copy performance goals. W4GNS goals can require a chosen
  speed for 1, 3, 5, or 10 minutes and give an immediate end recommendation.

## Phase 4: Feedback and statistics

- [x] Record response time and per-item accuracy in a versioned history format.
  Each completed item is stored locally; F7 shows current-callsign item
  accuracy, average answer time, and the most frequently missed item.
- [x] Add in-program score, speed, and accuracy trends without requiring
  gnuplot. F7 now shows local history summaries when available.
- [x] Add a persistent character-confusion view. F7 shows the most frequent
  sent-to-entered substitutions, omissions, and extra characters for the
  current callsign.
- [x] Add a frequently-missed-item view and optional confusion-focused drill.
  Focus mode selects loaded items containing symbols from the operator's most
  common copy differences and is explicitly toplist-ineligible.
- [x] Add a persistent spaced-review mode. It derives due items from the
  local item history using expanding success intervals and remains
  toplist-ineligible.
- [x] Add delayed-answer batch copy. It sends up to five calls before input,
  building short-term retention for contest and pileup operation; repeats are
  unavailable and these sessions are toplist-ineligible.
- Support history export and import.

## Phase 5: Advanced practice and UX

- [~] Configurable pitch range, volume, spacing, and independent rise/fall times.
  F5 now exposes volume, QRN, fixed/random pitch, and the common rise/fall time;
  independent shaping and additional spacing controls need audio design.
- [~] Optional QRM, QRN, fading, and pileup simulation. QRN, configurable
  QSB-style fading, and co-channel CW QRM are available; pileup remains.
- [~] Terminal resize handling, configurable keys, and accessible color themes.
  POSIX terminals now refresh ncurses state after SIGWINCH; configurable keys
  and themes remain.
- Headless practice generation and WAV export.
