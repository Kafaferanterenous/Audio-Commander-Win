# AudioCommander v9.3 responsiveness handoff - 2026-09-10

## Outcome

The reported large-folder freeze was reproduced and fixed. The sanitized
108-file test folder took 4,032 ms per pane on the old synchronous metadata path.
The final browser path listed the same 108 files in 62-78 ms and now fills
durations asynchronously without blocking folder navigation.

## Implementation

- `browser_list_folder` performs directory enumeration only.
- `metadata_loader` owns a cancellable background worker and posts small result
  batches to the window.
- Per-pane generations prevent results from an old folder being applied after
  navigation.
- Stable entry IDs and an ID-to-row index keep batch application efficient.
- Sorting uses bottom-up merge sort and no longer triggers folder reloading.
- FFmpeg metadata initialization is safe when the worker and UI request it
  concurrently.
- Bulk row rendering suppresses intermediate ListView redraws.

## Verification

- Release build: passed.
- Safe no-window CTest selection: 10/10 passed.
- Large-sort regression: 10,000 entries passed.
- Background submit/completion/cancellation regression: passed.
- Exact copied-folder enumeration: 108/108 recognized audio files, 62-78 ms.
- Portable package: 33 files; all 32 manifest hashes verified; no INI and no
  copied audio filenames or content present.

Live playback tests (7 CTest cases) and visible GUI acceptance were intentionally
not run without separate authorization for audio output and a GUI launch.

## Release artifacts

- `audiocommander_v93.exe`
  - SHA-256 `218DD6BF94B06FE671AF637AAF954CDF3F572C4C5012AC9CFF65BF2640C26892`
- `AudioCommander-v93-portable.zip`
  - SHA-256 `39162389E09EA5DB44B2A8824E45BCCDEEF383F30B4721A9DAC3ED5F0234B5C9`

The v9.2 executable and portable ZIP remain untouched.

## GitHub release update

- The private GitHub portable folder contains 31 files; all 30 listed package
  hashes verify. Two upstream source archives were omitted because their test
  trees contain media files.
- A screenshot was captured and visually reviewed from a fresh isolated copy
  pointed at two empty demo folders.
- No audio or music file was loaded, played, copied, staged, or uploaded during
  the screenshot and GitHub update.
