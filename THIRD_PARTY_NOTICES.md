# Third-party notices

## FFmpeg minimal audio decoder pack

AudioCommander optionally uses libraries from the FFmpeg project under the
GNU Lesser General Public License version 2.1 or later.

- FFmpeg commit: `c23123630e6a7e645c199599b8ade3fe7e9ab3db`
- FFmpeg version: `N-125705-gc23123630e-20260720`
- Corresponding source: `third_party/FFmpeg-c23123630e-source.zip`
- Source archive SHA-256: `4E817C0037A973E3EEF52FD7F394A7CB8B545DB48CE74C61D73B574B3708F6D9`
- Licence text: `FFmpeg-LICENSE.txt`
- Build recipe: `third_party/build_ffmpeg_minimal.ps1`
- Configuration: `third_party/ffmpeg_minimal_config.txt`
- Upstream project: https://ffmpeg.org/

Runtime files:

- `avcodec-63.dll` — 742,400 bytes — SHA-256
  `F726700DEECCE1993F3E2FDA9D063B5690246F28B2FD5BF39048BC029554250F`
- `avformat-63.dll` — 486,400 bytes — SHA-256
  `F3F365CE9C3DE45720C7868B7B08A2AE5BF7740686BF98F5A5BC842A10D0C660`
- `avutil-61.dll` — 582,656 bytes — SHA-256
  `E01A290958C01B8A0786D5CCB35AFD1513980D5C33C7353B955467C746589259`
- `swresample-7.dll` — 76,800 bytes — SHA-256
  `B25D39BD736FF8656FEB3F74D1AD88773BA441207B6E5CCE0A0E79B7BE2552F4`

The four DLLs total 1,888,256 bytes. They were built without GPL, nonfree,
version-3-only, external-library, network, device, filter, or video-scaling
components. Runtime dependencies are limited to these four DLLs and Windows
system libraries (`KERNEL32.dll`, `msvcrt.dll`, and `bcrypt.dll`).

The decoder pack is delay-loaded. AudioCommander starts without it; Ogg, Opus,
and ALAC require the pack, while formats supported natively by Windows retain a
fallback path.

This notice is not legal advice.

## libopenmpt

AudioCommander statically links libopenmpt 0.8.7 to decode tracker modules.

- Supported AudioCommander tracker extensions: .mod, .s3m, and .xm
- Upstream release: libopenmpt-0.8.7+release.msvc.zip
- Source archive SHA-256:
  663BD0432A991FD6CEC2F96D34BD125A04BA0AB169FBF1276FD936889465AD42
- Licence: BSD 3-Clause; the complete text is provided as
  libopenmpt-LICENSE.txt
- Upstream project: https://lib.openmpt.org/libopenmpt/

The decoder and its small embedded helper libraries are linked into
audiocommander_v8.exe; there is no libopenmpt runtime DLL.
