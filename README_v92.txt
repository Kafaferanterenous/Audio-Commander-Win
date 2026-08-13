AudioCommander v9.2 portable
============================

Quick start
-----------

1. Keep every file, the skins folder, and the source folder together.
2. Run audiocommander_v92.exe.
3. Read the first-run notice. Choose Yes only if you accept that the program is
   provided as-is, without guarantees or warranties, and that you use it at
   your own risk. Choosing No exits without recording acceptance.
4. Press F1 in AudioCommander for controls and safety details.

No installer is required. After acceptance, audiocommander.ini is created
beside the executable. The package intentionally ships without an INI.

Version 9.2 slow-copy feedback
-------------------------------

Single-file copies now run on a worker thread so the AudioCommander window can
continue responding while a slow destination is writing. Copies that are still
active after 600 milliseconds display a progress window with the file name,
bytes copied, percentage, and a clear reminder to be patient because the
computer is still working. Quick copies finish without flashing a dialog.

This applies to individual file-copy operations. Directory copies continue to
use the standard Windows file-operation progress interface. AudioCommander
still never silently overwrites an existing destination.

Version 9.1 skins retained
--------------------------

Pastel is the default built-in theme. Windows Native is retained as the plain
recovery choice. The former Follow Windows, Light, Dark, Blue, and built-in
Office 2003 entries were removed from v9.1 and remain removed in v9.2.

Eight selected external palettes are in the skins folder:

- Acryl
- Air
- MetroUI
- Office 2003
- Office 2007 Black
- XP Luna
- XP Silver
- Zest

These editable .skn files style AudioCommander's client-area colours, gradient
buttons, file lists, headers, selections, summary bands, and progress bar.
They are restrained palette approximations, not pixel-identical Balabolka
AlphaSkins. Windows continues to draw the title bar, menus, scrollbars, and
standard glyphs. Missing or invalid skin files are omitted; an unavailable
saved skin falls back safely to Pastel.

Existing v9 settings migrate without reusing old numeric meanings. Old Pastel
and Windows Native choices are retained, old Office 2003 selects the external
Office 2003 palette when present, and removed themes become Pastel.

Playback, file safety, and portability
--------------------------------------

WAV, MP3, M4A/MP4 audio, FLAC, WMA, Ogg Vorbis, Opus, ALAC, MIDI, MOD, S3M,
and XM are supported when the required bundled or Windows decoder is
available. MOD/S3M/XM decoding uses statically linked libopenmpt. Optional
sequential playback follows the visible pane order.

AudioCommander never silently overwrites an existing destination. Normal
fixed-drive deletion requests the Recycle Bin. Removable-drive deletion is
permanent and receives a separate warning whose default answer is No.

The four FFmpeg DLLs must remain beside audiocommander_v92.exe. Exact matching
source archives, build recipes, licence files, and THIRD_PARTY_NOTICES.md are
included. PACKAGE_SHA256.txt records release-file hashes.

This software is provided as-is, without guarantees or warranties. Use it at
your own risk.
