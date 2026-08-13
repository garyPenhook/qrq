# QRQ modernization handoff

This file is the operational handoff for continuing work on
`garyPenhook/qrq`. The longer-term product plan is in `ROADMAP.md`.

## Repository state

- Working directory: `/home/gary/apps/qrq-master`
- Current branch: `master`
- Original fork commit: `4e673e4c70d52d207a23e5ef2240e54bb6aa82d2`
- Imported upstream commit: `6174eeebec97cf56a041663cd1558beddee75b36`
- Local baseline commit: `c590cd2845cd255884af2868d1653037c6a6c963`
- Audio/callbase hardening commit: `be3b586b8ba33e759e863c86d93514cd4d2fa4ec`
- `origin`: `https://github.com/garyPenhook/qrq.git`
- `upstream`: `https://git.fkurz.net/dj1yfk/qrq.git`
- Published to `origin/master` through `caee299`; later local commits are
  intentionally awaiting the next explicit publication request.

Important: do not discard or overwrite unrelated local work. Upstream and the
fork have unrelated Git histories, so the upstream tree was applied on top of
the fork commit as a reviewable content update. Upstream also moved program
sources into `src/` and removed the obsolete in-tree Debian packaging.

## Completed modernization work

- Imported official upstream master, including post-0.3.5 maintenance.
- Added `ROADMAP.md`.
- Restored correct Backspace behavior; upstream master deleted two characters.
- Restored the separate Delete-key path missing from upstream master.
- Fixed `speedstep` and `stoponerror` config persistence:
  - restored the missing comma between config keys;
  - expanded the option table and loop from 13 to 14 entries;
  - made the existing `case 13` reachable.
- Removed the duplicate `speedstep` entry from the sample config.
- Corrected the 28-character boundary by using `CALL_MAX + 1` buffers and a
  strict `< len` input-capacity check.
- Replaced non-standard `M_PI` usage with a portable constant.
- Corrected the `src/Makefile` ChangeLog path used for build metadata.

## Verification already performed

The OSS build succeeds with GCC 16:

```sh
make -C src USE_PA=NO clean all
make -C src USE_PA=NO clean
```

A strict C17 syntax check completes without errors after the PI fix, although
it reports inherited warnings:

```sh
cc -std=c17 -D_POSIX_C_SOURCE=200809L -DOSS \
  '-DDESTDIR="/usr"' '-DVERSION="0.3.5"' -I src \
  -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes \
  -Wmissing-prototypes -Werror=implicit-function-declaration \
  -fsyntax-only src/qrq.c src/oss.c
```

The OSS and PulseAudio backends both build locally.

## Immediate next tasks

1. [x] Create a local baseline commit before larger refactors. Do not push
   until the user requests publication.
2. [x] Replace `full_buf[882000]` in `src/qrq.c` with a capacity-checked
   dynamic sample buffer. Allocation failure now stops the transmission.
3. [x] Harden `src/pulseaudio.c`:
   - use a temporary pointer for `realloc`;
   - reject integer-size overflow;
   - do not mark the backend opened when `pa_simple_new()` fails;
   - check `pa_simple_write()` and `pa_simple_drain()` results.
   Verified locally with PulseAudio 17.0 development headers.
4. [x] Remove the `sending_complete` data race. POSIX builds use a mutex and
   Windows builds use Interlocked operations; repeat playback also resets the
   state before starting its worker.
5. [x] Fix callbase ownership. `read_callbase()` now tracks allocated rows,
   frees every string on reload, and safely cleans up after a partial
   allocation failure.
6. [x] Replace shell-based copies and fixed temporary paths. Startup copying
   now uses internal I/O; old-toplist conversion no longer uses `system()` or
   `/tmp/qrq-toplist`; statistics streams its script directly to `gnuplot`.
7. [x] Repair config/toplist allocation and short-I/O handling. The focused
   C99 `cppcheck` pass is clean after rewriting the config writer, score
   insertion, and old-toplist conversion.
8. [x] Write config and toplist updates through same-directory temporary
   files and atomically replace the destination after a complete flush.

## Known compiler/static-analysis debt

- The staged upstream files contain substantial pre-existing whitespace/CRLF
  noise; do not mix a repository-wide formatting pass into correctness fixes.

## After safety hardening

1. [x] Add configurable session lengths; 50 calls remains the comparable
   high-score default, with `[` and `]` changing the setting by five calls.
2. [~] Extract pure scoring, Morse timing, callbase, and configuration modules.
   Scoring and callbase parsing now live in focused modules; timing and config
   remain in the UI module.
3. [~] Add unit tests and sanitizer targets. Scoring tests run in CI alongside
   the existing sanitizer smoke test; callbase parsing and filters are covered
   too.
4. [x] Add GitHub Actions for GCC/Clang and OSS/PulseAudio builds, plus
   cppcheck and an ASan/UBSan smoke test. CI now also covers MinGW/MSYS2 and
   macOS/Core Audio builds.
5. [x] Add independent up/down speed steps; legacy `speedstep` configs remain
   compatible, while `speedupstep` and `speeddownstep` persist separately.
6. [x] Add an optional accuracy-target policy. Cycle it with `g` in F5, or
   set `accuracytarget` to 0 or 50–100 in `qrqrc`.
7. [x] Add missed-item review and adaptive selection. Enable
   `adaptiveselection=1` to weight difficult calls, or `reviewmisses=1` to
   schedule copied-wrong calls again in the same session.
8. [x] Add reproducible practice sessions via a nonzero `sessionseed`.
9. [x] Add filters for call length, prefixes, digits, portable suffixes, and
   characters. See `mincalllength`, `maxcalllength`, `callprefixes`,
   `digitmode`, `portablemode`, and `allowedchars` in `qrqrc`.
10. [x] Separate the runtime install prefix from package staging. `PREFIX`
   defaults to `/usr`; `DESTDIR` can now stage a package tree safely.
11. [~] Record versioned local session history in `<toplist>.history.csv`;
    per-item response timing and analytics remain.
12. [x] Show score, speed, and accuracy history in-program with F7; gnuplot
    remains a fallback when no local history exists.
13. Future product features—advanced
   audio, and terminal UX—are tracked in `ROADMAP.md` and need a product-level
   design decision before implementation.
9. [x] Add a warning-free strict C17 syntax gate with `-Werror`.

## Follow-up from the 2026-08-13 operational and code review

The review passed the focused ASan/UBSan, Valgrind, GCC/Clang, and staging
checks. The items below are the remaining actionable findings, ordered by
priority.

### High priority

- [x] Make `qrqscore` fail safely when the toplist servers are unavailable.
  Limit retries to the declared endpoints, set an explicit network timeout,
  report a final error, and never continue with an undefined URL. Validate
  every downloaded record against the local fixed-width toplist format; hold a
  file lock and replace the local file atomically only after a complete,
  validated merge. Preserve an existing backup instead of silently overwriting
  it, and select the upload row by exact callsign rather than a substring.
- [x] Make sustained-copy goals continue until their configured duration
  expires. The current loop caps sustained mode to one pass through the
  callbase, so a one-item custom drill ends almost immediately. Permit
  replacement after a full pass while preserving no-duplicate behavior within
  an answer batch. A PTY regression test covers a one-item custom drill that
  advances into a second pass; small custom drills, generated serials, and
  duration/speed-goal completion remain under the integration-harness item
  below.
- [x] Honor `DESTDIR` in the non-bundle `uninstall` target. Today a staged
  `make uninstall DESTDIR=...` removes files from the live `PREFIX` instead of
  the stage tree. Cover staged install/uninstall with an automated packaging
  check.

### Medium priority

- [x] Install the FreeDesktop launcher and icon on Linux. The repository ships
  `src/qrq.desktop` and `src/qrq.png`, but `make install` and `make dist` omit
  the desktop integration. Install the desktop file under
  `share/applications`, install a correctly named icon in an appropriate icon
  hierarchy, and remove both during uninstall.
- [x] Make the production build as strict and reproducible as CI. Define the
  C language standard and baseline warning set for `all`, add dependency-file
  generation for headers, and give `append_summary()` a compiler printf-format
  annotation so `-Wformat=2 -Werror` succeeds and checks every caller.
- [x] Repair the macOS bundle launcher for paths containing spaces. Replace
  its unquoted backtick/sed path extraction with a quoted `dirname`-based
  launcher that `exec`s `/usr/bin/open`.

### Test and operational coverage

- [x] Add a mock audio backend and PTY-driven smoke test for the main ncurses
  workflow. It now covers startup, one real session, summary display, and
  orderly shutdown without sound hardware.
- [ ] Extend PTY coverage to settings persistence and duration/speed-goal
  completion. Three-digit serial and two-item custom-drill sessions verify
  generator/custom modes and complete their configured items. Retain platform
  compile jobs, then add backend-appropriate runtime/failure tests for
  PulseAudio, OSS, Core Audio, and Windows/PDCurses.

## Useful inspection commands

```sh
git status --short
git diff --cached --stat
git diff --cached -- src/qrq.c src/Makefile src/qrqrc ROADMAP.md TODO.md
git log --oneline --decorate --graph --all --max-count=20
```

## Full code and option review backlog (2026-08-05)

Scope: all C sources and headers, Linux OSS/PulseAudio paths, static review of
the Windows/Core Audio paths, the Makefile and CI workflow, bundled callbases,
configuration parsing/persistence, F5 controls, scoring, summaries, toplist,
and history. The existing core tests and manual ASan/UBSan runs pass; the items
below are gaps those tests do not currently exercise.

### High priority: correctness, safety, and data integrity

- [x] Separate the cumulative mistake count from the error-display cursor.
  The UI now wraps an independent display counter without changing session
  errors, accuracy, history, or `accuracytarget` evaluation.
- [x] Make adaptive selection effective. Missed items now remain in the
  available pool with increased weight until they are copied correctly; the
  result transition and saturation behavior have focused tests.
- [x] Exclude aborted and incomplete comparable sessions from the toplist.
  Eligibility now requires ordinary scoring, every requested call completed,
  and the optional accuracy target; history records that policy independently
  of whether the resulting numeric score is zero.
- [x] Replace `read_config()` with a bounded key/value parser and unit tests.
  In particular:
  - do not delete the final value character when the last line lacks a newline;
  - accept CRLF and trailing whitespace/comments consistently;
  - replace `atoi()`/`atof()` and unchecked `ctype` calls with checked parsing;
  - reject overflow and nonsensical speed, sample-rate, pitch, edge, and length
    combinations before they reach timing or allocation arithmetic;
  - validate related options after parsing the whole file so file order does
    not change `minpitch`/`maxpitch`, length, or legacy speed-step behavior.
- [x] Fix `sessionseed` persistence. A tested unsigned-value helper now accepts
  saved trailing whitespace, CRLF, and optional comments while rejecting signs,
  overflow, and trailing junk.
- [x] Bound path construction and callbase discovery. `find_files()` and
  `find_callbases()` now use checked construction and environment fallbacks;
  the list is count-bounded and every probe file and directory is closed.
- [x] Validate Morse timing arithmetic. User-controlled speed, `samplerate`,
  `mincharspeed`, and rise time now have defined ranges; runtime shaping caps
  the edge below the generated dot so silence lengths remain nonnegative.
- [x] Prevent signed session-score overflow and define the supported score
  range. Accumulation now saturates at the fixed-width toplist limit of 999999,
  and the writer rejects any out-of-contract value explicitly.
- [x] Prevent F5 from mutating audio globals while the worker is playing. A
  centralized, checked worker lifecycle now joins active playback before the
  settings dialog can read or change transmission parameters, and editing an
  unrelated option no longer resets the current session speed.
- [x] Repair Windows audio/thread error handling. Worker creation, waits, and
  handles now have one checked path; WinMM validates event creation, device
  opening, buffer preparation/playback, completion, and cleanup.
- [x] Harden Core Audio initialization and playback synchronization. Every
  initialization step is checked with partial cleanup, the configured sample
  rate is honored, and the render callback now only copies/zeros samples and
  publishes completion instead of taking locks or stopping its AudioUnit.
- [x] Handle partial/interrupted OSS writes and propagate audio failures. OSS
  now completes short writes and retries `EINTR`; PulseAudio, Core Audio, and
  Windows report their playback/setup failures through checked return paths.

### Medium priority: training and option behavior

- [x] Make speed changes understandable in the UI. The live score line now
  displays overall speed and effective character speed separately, and F5 calls
  `mincharspeed` the character-speed floor so Farnsworth spacing is visible.
- [x] Define deliberate review scheduling. `reviewmisses=1` now reserves every
  third transmission for the oldest pending miss, guaranteeing intervening new
  material and preventing a repeatedly missed item from starving unseen items.
- [x] Correct word-space timing. A space is treated as an extra character gap,
  It now adds four units after the preceding three-unit character gap, producing
  the standard seven-unit gap in ordinary and Farnsworth timing.
- [x] Validate callbase characters against symbols the player and input editor
  both support. Unsupported rows, including the bundled `cwops.qcb` header, are
  skipped instead of being transmitted as an unenterable question-mark pattern.
- [x] Make callsign and path editing round-trip. The bounded editor now has a
  horizontal viewport for full-length OSS paths and accepts literal path
  punctuation; config saves preserve spaced values, formatting, and comments.
- [x] Separate deterministic practice randomness from QRN sample generation.
  QRN uses a private generator, so audio sample generation no longer consumes
  the seeded item/pitch selection sequence.
- [x] Exclude ineligible training sessions from comparable history summaries.
  Fixed-speed, unlimited, review, adaptive, and seeded sessions remain recorded
  but no longer depress average scores or distort score trends.
- [x] Make session eligibility monotonic. Once a running attempt enables a
  non-comparable mode, turning it off again cannot restore toplist eligibility.
- [x] Make duplicate config keys save predictably. Since loading is
  last-value-wins, F2 now updates the effective last assignment instead of an
  earlier shadowed value.
- [x] Reject corrupt history headers, incomplete final records, trailing CSV
  fields, and oversized callsigns instead of appending or counting ambiguous
  data.
- [x] Repair an empty toplist with its required header, reject invalid record
  widths at startup, and convert both LF and CRLF legacy toplists safely.
- [x] Bound macOS bundle resource-path derivation and remove incomplete
  first-run resource copies after read, write, or close failures.
- [x] Use collision-resistant summary filenames. Summaries include seconds and
  a securely exclusive `mkstemp()` suffix; incomplete writes/close failures are
  reported and the partial new file is removed.
- [x] Match toplist/history records by the fixed callsign field, not `strstr()`.
  Highlighting, “own scores,” and legacy statistics now compare the exact
  ten-byte space-padded field, with focused near-match tests.
- [x] Make callbase counts use `size_t` end-to-end. The loaded count is no
  longer narrowed through `int`/`unsigned long` before the session-size guard.
- [x] Close history files even after read errors and reject accumulation or
  session-count overflow in very large or untrusted histories.

### F5 screen and usability

- [x] Fix the session-length layout (`or u50` in the old screen). The numeric or
  `all` value is now rendered in the value column before the key hint.
- [x] Show the `stoponerror` state and its existing `t` key alongside fixed
  speed so the setting no longer changes invisibly.
- [x] Expose config-only options through three F5 pages. Volume, QRN, random
  pitch range, call length/prefix/digit/portable/character filters, and session
  seed are editable. `samplerate` is shown as read-only, documented in the
  sample config, and now persists when F2 saves other settings.
- [x] Prevent integer overflow in repeated F5 key adjustments for initial
  speed, minimum character speed, and speed steps, and show enforced limits.
- [x] Make the callbase chooser bounded and cancellable, deduplicate results,
  close directory handles, use exact case-insensitive `.qcb` suffix matching,
  and avoid loading the selected database twice.
- [x] Check terminal dimensions and every `newwin()` result before drawing.
  The fixed layout now enforces its documented 80x24 minimum at startup and
  after POSIX resize events, and reports window-allocation failures cleanly.

### Build, test, and documentation follow-up

- [~] Add focused tests for config read/save round trips, CRLF/no-final-newline
  files, every option boundary, speed-down plus Farnsworth timing, word gaps,
  cumulative error counts past display wrap, abort eligibility, adaptive/review
  scheduling, summary collisions, and malformed toplist files. Config value
  replacement now covers spaced paths, duplicate keys, inline comments, CRLF,
  appending, and a final line without a newline; history tests cover CRLF,
  invalid headers, trailing fields, and incomplete records. The remaining
  UI/audio cases still need extraction or an integration harness.
- [x] Exercise unit tests under ASan/UBSan in CI. The sanitizer job currently
  runs the focused suites as well as `qrq -h`, and Makefile test recipes honor
  caller-provided sanitizer compiler flags.
- [~] Extend the strict warning gate to PulseAudio and platform backends. The
  OSS and PulseAudio sources now pass it. Add mocked/runtime-appropriate Windows
  and Core Audio failure-path tests.
- [x] Update CI's hard-coded version (`0.3.5`) to the Makefile version and add a
  single-source version check.
- [x] Refresh README, INSTALL, and the man page to enumerate current controls,
  speed bounds, settings pages, history, eligibility rules, MSYS2 build steps,
  and the runtime `PREFIX`/packaging `DESTDIR` distinction.
- [x] Add a conventional `test` alias for the existing `check` target so the
  documented/common `make test` workflow does not fail.
