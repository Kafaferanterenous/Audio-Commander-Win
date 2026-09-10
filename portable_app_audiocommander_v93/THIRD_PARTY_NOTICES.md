# Third-party notices for AudioCommander v9.3

## FFmpeg minimal audio decoder pack

AudioCommander uses libraries from the FFmpeg project under the GNU Lesser
General Public License version 2.1 or later. The package includes the complete
licence text as `FFmpeg-LICENSE.txt`, the build recipe, configuration, exact
source commit, and upstream project link. The corresponding source archive is
not in this sanitized GitHub snapshot because its test tree contains media
fixtures. The runtime files are `avcodec-63.dll`,
`avformat-63.dll`, `avutil-61.dll`, and `swresample-7.dll`.

- FFmpeg commit: `c23123630e6a7e645c199599b8ade3fe7e9ab3db`
- Upstream project: https://ffmpeg.org/

## libopenmpt and helper libraries

AudioCommander uses libopenmpt 0.8.7 and its separately distributed helper
DLLs for MOD, S3M, and XM playback. The package includes the runtime libraries
and the licence and attribution texts supplied with the matching upstream MSVC
distribution. The distribution archive is not in this sanitized GitHub
snapshot because its test tree contains audio fixtures.

- libopenmpt: BSD 3-Clause (`libopenmpt-LICENSE.txt`)
- mpg123: LGPL 2.1 (`mpg123-LICENSE.txt`, `mpg123-AUTHORS.txt`)
- libogg: BSD-style licence (`ogg-LICENSE.txt`)
- libvorbis: BSD-style licence (`vorbis-LICENSE.txt`)
- zlib: zlib licence (`zlib-LICENSE.txt`)
- Upstream project: https://lib.openmpt.org/libopenmpt/

`PACKAGE_SHA256.txt` records the exact hash of every distributed file.

This notice is not legal advice.
