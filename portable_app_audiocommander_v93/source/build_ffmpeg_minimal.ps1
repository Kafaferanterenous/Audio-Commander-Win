param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$thirdParty = Join-Path $ProjectRoot 'third_party'
$kitBin = Join-Path $thirdParty 'w64devkit-2.8.0\w64devkit\bin'
$bash = Join-Path $kitBin 'bash.exe'
$source = Join-Path $thirdParty 'ffmpeg-minimal-source\FFmpeg-c23123630e6a7e645c199599b8ade3fe7e9ab3db'
$build = Join-Path $thirdParty 'ffmpeg-minimal-build'
$pack = Join-Path $thirdParty 'ffmpeg-minimal-pack'
$temp = Join-Path $thirdParty 'ffmpeg-minimal-tmp'

foreach ($required in $bash, (Join-Path $source 'configure')) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing required path: $required" }
}
New-Item -ItemType Directory -Path $build, $pack, $temp -Force | Out-Null

$rootPosix = $ProjectRoot.Replace('\', '/')
$command = @"
export PATH="$rootPosix/third_party/w64devkit-2.8.0/w64devkit/bin;`$PATH"
export TMPDIR="$rootPosix/third_party/ffmpeg-minimal-tmp"
cd "$rootPosix/third_party/ffmpeg-minimal-build"
"$rootPosix/third_party/ffmpeg-minimal-source/FFmpeg-c23123630e6a7e645c199599b8ade3fe7e9ab3db/configure" \
  --prefix="$rootPosix/third_party/ffmpeg-minimal-pack" \
  --arch=x86_64 --target-os=mingw32 --enable-shared --disable-static \
  --disable-everything --disable-autodetect --disable-programs --disable-doc \
  --disable-debug --disable-network --disable-x86asm --enable-small \
  --disable-avdevice --disable-avfilter --disable-swscale \
  --enable-avcodec --enable-avformat --enable-avutil --enable-swresample \
  --enable-protocol=file --enable-demuxer=asf,flac,mov,mp3,ogg,wav \
  --enable-parser=aac,flac,mpegaudio,opus,vorbis \
  --enable-decoder=aac,alac,flac,mp3,opus,pcm_s16le,vorbis,wmav1,wmav2
make V=1 LN_S="cp -f" -j8
make V=1 LN_S=cp install
"@

& $bash -c $command
if ($LASTEXITCODE -ne 0) { throw "FFmpeg build failed with exit code $LASTEXITCODE" }
