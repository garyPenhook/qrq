# QRQ CW skills backlog

This is the feature backlog for making QRQ a more effective deliberate-practice
tool while preserving its fast, keyboard-first contest trainer workflow.

## Feedback and personalization

- [x] Record character substitutions, omissions, and extra characters from
  incorrect copies; show the most frequent differences in F7 statistics.
- [x] Record per-item response time, excluding the time spent replaying an
  item; F7 shows the all-time average answer time for the current callsign.
- [x] Add a per-item accuracy view. F7 highlights the most frequently missed
  call or word alongside overall item accuracy.
- [ ] Add configurable recency weighting and a reset command for old
  confusion data, so past mistakes do not hide current progress.
- [x] Offer a confusion-focused drill. The optional `focusconfusions` mode
  limits the next session to loaded calls or words containing symbols from
  the user's most frequent copy differences.

## Practice modes

- [x] Add persistent spaced review. Wrong items are due in the next session;
  successful items return after 1, 3, 7, 15, then 31 subsequent attempts.
- [~] Provide focused generators for digits, serial exchanges, prefixes,
  portable calls, prosigns, punctuation, and user-supplied text. Zero-padded
  three-, four-, and five-digit serial exchanges are available; the remaining
  generators still need dedicated modes.
- [x] Add a delayed-answer batch mode that sends two to five items before
  accepting input, for pileup and short-term-copy practice.
- [ ] Provide a goal mode such as "20 items at 95%" or "maintain 35 WpM for
  five minutes", with a concise end-of-session recommendation.

## Realism and accessibility

- [~] Add independently configurable QRM, fading/QSB, and pileup simulation;
  QRN, co-channel CW QRM, and QSB fade depth are independent. Pileup remains.
  Keep the clean-signal mode available for controlled practice.
- [ ] Add separate character-, word-, and extra-space timing controls with
  clear Farnsworth labels.
- [ ] Add audio calibration, pitch sweep, and listening-comfort presets.
- [ ] Add configurable keys and high-contrast terminal color themes.

## Portability and data

- [ ] Add CSV export/import for session, response-time, and confusion data.
- [ ] Add headless practice generation and WAV export with delayed answer
  sheets for offline practice.

## Product constraints

- Keep ordinary 50-call sessions comparable with the existing toplist.
- Make training-only modes explicit and preserve the current no-mouse flow.
- Prefer local, inspectable data files and test every new persistence format.
