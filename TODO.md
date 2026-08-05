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
8. [x] Add filters for call length, prefixes, digits, portable suffixes, and
   characters. See `mincalllength`, `maxcalllength`, `callprefixes`,
   `digitmode`, `portablemode`, and `allowedchars` in `qrqrc`.
9. [x] Separate the runtime install prefix from package staging. `PREFIX`
   defaults to `/usr`; `DESTDIR` can now stage a package tree safely.
10. [~] Record versioned local session history in `<toplist>.history.csv`;
    per-item response timing and analytics remain.
11. Future product features—advanced
   audio, and terminal UX—are tracked in `ROADMAP.md` and need a product-level
   design decision before implementation.
9. [x] Add a warning-free strict C17 syntax gate with `-Werror`.

## Useful inspection commands

```sh
git status --short
git diff --cached --stat
git diff --cached -- src/qrq.c src/Makefile src/qrqrc ROADMAP.md TODO.md
git log --oneline --decorate --graph --all --max-count=20
```
