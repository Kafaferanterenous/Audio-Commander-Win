AudioCommander v9.3 portable
============================

USE AT YOUR OWN RISK. This experimental software is provided as-is, without
guarantees or warranties. Back up important data before use.

Quick start
-----------

1. Keep every file and the skins folder together.
2. Run audiocommander_v93.exe.
3. Read the first-run notice. Choose Yes only if you accept that the program is
   provided as-is, without guarantees or warranties, and that you use it at
   your own risk. Choosing No exits without recording acceptance.
4. Press F1 in AudioCommander for controls and safety details.

No installer is required. After acceptance, audiocommander.ini is created
beside the executable. The package intentionally ships without an INI.

Version 9.3 large-folder responsiveness
----------------------------------------

Folder rows now appear after fast directory enumeration. Audio durations are
read by a cancellable background worker and applied in small batches, so a
folder containing many audio files no longer blocks the window while every
file is inspected. Changing folders cancels stale work. Sorting is O(n log n),
and duration sorting is refreshed when the background pass completes.

Version 9.2's responsive single-file copying and v9.1's eight external colour
palettes are retained.

Playback, file safety, and portability
--------------------------------------

WAV, MP3, M4A/MP4 audio, FLAC, WMA, Ogg Vorbis, Opus, ALAC, MIDI, MOD, S3M,
and XM are supported when the required bundled or Windows decoder is
available. The nine runtime DLLs listed in PORTABLE_CONTENTS.txt must remain
beside audiocommander_v93.exe.

AudioCommander never silently overwrites an existing destination. Normal
fixed-drive deletion requests the Recycle Bin. Removable-drive deletion is
permanent and receives a separate warning whose default answer is No.

Licence files, notices, exact dependency versions, and upstream project links
are included. Dependency source archives are not included in this sanitized
GitHub snapshot because upstream test trees contain audio/media fixtures.
PACKAGE_SHA256.txt records release-file hashes.

This software is provided as-is, without guarantees or warranties. Use it at
your own risk.
