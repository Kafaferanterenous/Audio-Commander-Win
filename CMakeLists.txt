cmake_minimum_required(VERSION 3.20)
project(AudioCommander LANGUAGES C)

set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/third_party/ffmpeg-minimal-pack")
set(OPENMPT_SOURCE_ROOT "${CMAKE_SOURCE_DIR}/third_party/libopenmpt-0.8.7-source")
set(OPENMPT_DEV_ROOT "${CMAKE_SOURCE_DIR}/third_party/libopenmpt-0.8.7-dev")

if(EXISTS "${OPENMPT_SOURCE_ROOT}/build/lib/vs2022win10/x86_64/Release/libopenmpt-small.lib")
    set(OPENMPT_INCLUDE_ROOT "${OPENMPT_SOURCE_ROOT}")
    set(OPENMPT_LIBRARIES
        "${OPENMPT_SOURCE_ROOT}/build/lib/vs2022win10/x86_64/Release/libopenmpt-small.lib"
        "${OPENMPT_SOURCE_ROOT}/build/lib/vs2022win10/x86_64/Release/openmpt-minimp3.lib"
        "${OPENMPT_SOURCE_ROOT}/build/lib/vs2022win10/x86_64/Release/openmpt-miniz.lib"
        "${OPENMPT_SOURCE_ROOT}/build/lib/vs2022win10/x86_64/Release/openmpt-stb_vorbis.lib")
    set(OPENMPT_DYNAMIC FALSE)
elseif(EXISTS "${OPENMPT_DEV_ROOT}/lib/amd64/libopenmpt.lib")
    set(OPENMPT_INCLUDE_ROOT "${OPENMPT_DEV_ROOT}/inc")
    set(OPENMPT_LIBRARIES "${OPENMPT_DEV_ROOT}/lib/amd64/libopenmpt.lib")
    set(OPENMPT_DYNAMIC TRUE)
else()
    message(FATAL_ERROR "Neither the static libopenmpt build nor the local development package is available")
endif()

add_library(audiocommander_tracker STATIC src/tracker_support.c)
target_compile_features(audiocommander_tracker PRIVATE c_std_11)
target_compile_definitions(audiocommander_tracker PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_include_directories(audiocommander_tracker PUBLIC
    src "${OPENMPT_INCLUDE_ROOT}")
target_link_libraries(audiocommander_tracker PUBLIC ${OPENMPT_LIBRARIES})
if(MSVC)
    set_property(TARGET audiocommander_tracker PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(audiocommander_tracker PRIVATE /W4 /permissive-)
endif()

add_executable(audiocommander_v93 WIN32
    src/app.c
    src/browser.c
    src/playlist.c
    src/playback.c
    src/settings.c
    src/skin.c
    src/media_info.c
    src/file_ops.c
    src/copy_progress.c
    src/localization.c
    src/metadata_loader.c
    src/playback_mf.c
    src/playback_ffmpeg.c
    src/playback_openmpt.c
    src/app.rc
)

target_compile_features(audiocommander_v93 PRIVATE c_std_11)
target_compile_definitions(audiocommander_v93 PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_include_directories(audiocommander_v93 PRIVATE "${FFMPEG_ROOT}/include")
target_link_directories(audiocommander_v93 PRIVATE "${FFMPEG_ROOT}/bin")
target_link_libraries(audiocommander_v93 PRIVATE audiocommander_tracker comctl32 comdlg32 shell32 propsys uuid winmm mfplay mfplat mfuuid ole32 uxtheme avformat avcodec avutil swresample)

if(MSVC)
    set_property(TARGET audiocommander_v93 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(audiocommander_v93 PRIVATE /W4 /permissive- /utf-8)
    target_link_libraries(audiocommander_v93 PRIVATE delayimp)
    target_link_options(audiocommander_v93 PRIVATE
        /DELAYLOAD:avformat-63.dll /DELAYLOAD:avcodec-63.dll
        /DELAYLOAD:avutil-61.dll /DELAYLOAD:swresample-7.dll)
endif()

foreach(runtime IN ITEMS avcodec-63.dll avformat-63.dll avutil-61.dll swresample-7.dll)
    add_custom_command(TARGET audiocommander_v93 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_ROOT}/bin/${runtime}" "$<TARGET_FILE_DIR:audiocommander_v93>/${runtime}")
endforeach()
if(OPENMPT_DYNAMIC)
    foreach(runtime IN ITEMS libopenmpt.dll openmpt-mpg123.dll openmpt-ogg.dll
                             openmpt-vorbis.dll openmpt-zlib.dll)
        add_custom_command(TARGET audiocommander_v93 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENMPT_DEV_ROOT}/bin/amd64/${runtime}"
            "$<TARGET_FILE_DIR:audiocommander_v93>/${runtime}")
    endforeach()
endif()

file(COPY "${CMAKE_SOURCE_DIR}/skins" DESTINATION "${CMAKE_BINARY_DIR}")

include(CTest)
if(BUILD_TESTING)
    add_executable(playback_smoke
        tests/playback_smoke.c
        src/playback.c
        src/playback_mf.c
        src/playback_ffmpeg.c
        src/playback_openmpt.c
    )
    target_include_directories(playback_smoke PRIVATE src)
    target_compile_features(playback_smoke PRIVATE c_std_11)
    target_compile_definitions(playback_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_include_directories(playback_smoke PRIVATE "${FFMPEG_ROOT}/include")
    target_link_directories(playback_smoke PRIVATE "${FFMPEG_ROOT}/bin")
    target_link_libraries(playback_smoke PRIVATE audiocommander_tracker winmm mfplay mfplat mfuuid ole32 avformat avcodec avutil swresample)
    if(MSVC)
        set_property(TARGET playback_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(playback_smoke PRIVATE /W4 /permissive-)
        target_link_libraries(playback_smoke PRIVATE delayimp)
        target_link_options(playback_smoke PRIVATE
            /DELAYLOAD:avformat-63.dll /DELAYLOAD:avcodec-63.dll
            /DELAYLOAD:avutil-61.dll /DELAYLOAD:swresample-7.dll)
    endif()

    add_executable(media_info_smoke tests/media_info_smoke.c src/media_info.c)
    target_include_directories(media_info_smoke PRIVATE src "${FFMPEG_ROOT}/include")
    target_link_directories(media_info_smoke PRIVATE "${FFMPEG_ROOT}/bin")
    target_compile_features(media_info_smoke PRIVATE c_std_11)
    target_compile_definitions(media_info_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(media_info_smoke PRIVATE audiocommander_tracker shell32 propsys uuid ole32 avformat avcodec avutil)
    if(MSVC)
        set_property(TARGET media_info_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(media_info_smoke PRIVATE /W4 /permissive-)
        target_link_libraries(media_info_smoke PRIVATE delayimp)
        target_link_options(media_info_smoke PRIVATE
            /DELAYLOAD:avformat-63.dll /DELAYLOAD:avcodec-63.dll /DELAYLOAD:avutil-61.dll)
    endif()

    add_executable(playback_stress
        tests/playback_stress.c src/playback.c src/playback_mf.c src/playback_ffmpeg.c
        src/playback_openmpt.c)
    target_include_directories(playback_stress PRIVATE src "${FFMPEG_ROOT}/include")
    target_link_directories(playback_stress PRIVATE "${FFMPEG_ROOT}/bin")
    target_compile_features(playback_stress PRIVATE c_std_11)
    target_compile_definitions(playback_stress PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(playback_stress PRIVATE audiocommander_tracker winmm mfplay mfplat mfuuid ole32
        avformat avcodec avutil swresample)

    add_executable(playback_seek_smoke
        tests/playback_seek_smoke.c src/playback.c src/playback_mf.c src/playback_ffmpeg.c
        src/playback_openmpt.c)
    target_include_directories(playback_seek_smoke PRIVATE src "${FFMPEG_ROOT}/include")
    target_link_directories(playback_seek_smoke PRIVATE "${FFMPEG_ROOT}/bin")
    target_compile_features(playback_seek_smoke PRIVATE c_std_11)
    target_compile_definitions(playback_seek_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(playback_seek_smoke PRIVATE audiocommander_tracker winmm mfplay mfplat mfuuid ole32
        avformat avcodec avutil swresample)

    add_executable(tracker_corrupt_smoke
        tests/tracker_corrupt_smoke.c
        src/media_info.c
        src/playback.c
        src/playback_mf.c
        src/playback_ffmpeg.c
        src/playback_openmpt.c)
    target_include_directories(tracker_corrupt_smoke PRIVATE
        src "${FFMPEG_ROOT}/include")
    target_link_directories(tracker_corrupt_smoke PRIVATE "${FFMPEG_ROOT}/bin")
    target_compile_features(tracker_corrupt_smoke PRIVATE c_std_11)
    target_compile_definitions(tracker_corrupt_smoke PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(tracker_corrupt_smoke PRIVATE audiocommander_tracker
        shell32 propsys uuid winmm mfplay mfplat mfuuid ole32
        avformat avcodec avutil swresample)

    add_executable(file_ops_smoke tests/file_ops_smoke.c src/file_ops.c)
    target_include_directories(file_ops_smoke PRIVATE src)
    target_compile_features(file_ops_smoke PRIVATE c_std_11)
    target_compile_definitions(file_ops_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(file_ops_smoke PRIVATE shell32)

    add_executable(copy_progress_smoke
        tests/copy_progress_smoke.c src/copy_progress.c src/file_ops.c)
    target_include_directories(copy_progress_smoke PRIVATE src)
    target_compile_features(copy_progress_smoke PRIVATE c_std_11)
    target_compile_definitions(copy_progress_smoke PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(copy_progress_smoke PRIVATE comctl32 shell32)

    add_executable(usb_copy_live_smoke
        tests/usb_copy_live_smoke.c src/copy_progress.c src/file_ops.c)
    target_include_directories(usb_copy_live_smoke PRIVATE src)
    target_compile_features(usb_copy_live_smoke PRIVATE c_std_11)
    target_compile_definitions(usb_copy_live_smoke PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(usb_copy_live_smoke PRIVATE comctl32 shell32)

    add_executable(settings_smoke tests/settings_smoke.c src/settings.c src/skin.c)
    target_include_directories(settings_smoke PRIVATE src)
    target_compile_features(settings_smoke PRIVATE c_std_11)
    target_compile_definitions(settings_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(settings_smoke PRIVATE gdi32 user32)

    add_executable(browser_sort_smoke
        tests/browser_sort_smoke.c src/browser.c)
    target_include_directories(browser_sort_smoke PRIVATE src)
    target_compile_features(browser_sort_smoke PRIVATE c_std_11)
    target_compile_definitions(browser_sort_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

    add_executable(playlist_smoke tests/playlist_smoke.c src/playlist.c)
    target_include_directories(playlist_smoke PRIVATE src)
    target_compile_features(playlist_smoke PRIVATE c_std_11)
    target_compile_definitions(playlist_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

    add_executable(metadata_loader_smoke
        tests/metadata_loader_smoke.c src/metadata_loader.c)
    target_include_directories(metadata_loader_smoke PRIVATE src)
    target_compile_features(metadata_loader_smoke PRIVATE c_std_11)
    target_compile_definitions(metadata_loader_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

    add_executable(sequential_playback_smoke
        tests/sequential_playback_smoke.c
        src/playlist.c
        src/playback.c
        src/playback_mf.c
        src/playback_ffmpeg.c
        src/playback_openmpt.c)
    target_include_directories(sequential_playback_smoke PRIVATE
        src "${FFMPEG_ROOT}/include")
    target_link_directories(sequential_playback_smoke PRIVATE "${FFMPEG_ROOT}/bin")
    target_compile_features(sequential_playback_smoke PRIVATE c_std_11)
    target_compile_definitions(sequential_playback_smoke PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(sequential_playback_smoke PRIVATE audiocommander_tracker winmm mfplay mfplat mfuuid ole32
        avformat avcodec avutil swresample)

    add_executable(localization_smoke
        tests/localization_smoke.c src/localization.c)
    target_include_directories(localization_smoke PRIVATE src)
    target_compile_features(localization_smoke PRIVATE c_std_11)
    target_compile_definitions(localization_smoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

    add_executable(ui_command_helper tests/ui_command_helper.c)
    target_compile_features(ui_command_helper PRIVATE c_std_11)
    target_compile_definitions(ui_command_helper PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(ui_command_helper PRIVATE user32)

    add_test(NAME v93_settings_smoke COMMAND settings_smoke)
    add_test(NAME v93_file_ops_policy_smoke COMMAND file_ops_smoke)
    add_test(NAME v93_copy_progress_smoke COMMAND copy_progress_smoke)
    add_test(NAME v93_browser_sort_smoke COMMAND browser_sort_smoke)
    add_test(NAME v93_playlist_smoke COMMAND playlist_smoke)
    add_test(NAME v93_metadata_loader_smoke COMMAND metadata_loader_smoke)
    add_test(NAME v93_sequential_playback_smoke COMMAND sequential_playback_smoke)
    add_test(NAME v93_playback_seek_smoke COMMAND playback_seek_smoke)
    add_test(NAME v93_playback_stress COMMAND playback_stress)
    add_test(NAME v93_localization_smoke COMMAND localization_smoke)
    add_test(NAME v93_mod_playback_smoke COMMAND playback_smoke
        "${CMAKE_SOURCE_DIR}/Audio/AXELF.MOD" 3000)
    add_test(NAME v93_s3m_playback_smoke COMMAND playback_smoke
        "${CMAKE_SOURCE_DIR}/Audio/AQUA.S3M" 3000)
    add_test(NAME v93_xm_playback_smoke COMMAND playback_smoke
        "${CMAKE_SOURCE_DIR}/Audio/BUTTERFL.XM" 3000)
    add_test(NAME v93_mod_media_info_smoke COMMAND media_info_smoke
        "${CMAKE_SOURCE_DIR}/Audio/AXELF.MOD" 637440)
    add_test(NAME v93_s3m_media_info_smoke COMMAND media_info_smoke
        "${CMAKE_SOURCE_DIR}/Audio/AQUA.S3M" 152080)
    add_test(NAME v93_xm_media_info_smoke COMMAND media_info_smoke
        "${CMAKE_SOURCE_DIR}/Audio/BUTTERFL.XM" 173275)
    add_test(NAME v93_corrupt_tracker_smoke COMMAND tracker_corrupt_smoke)

    if(MSVC)
        foreach(test_target playback_stress playback_seek_smoke
                            sequential_playback_smoke tracker_corrupt_smoke)
            set_property(TARGET ${test_target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
            target_compile_options(${test_target} PRIVATE /W4 /permissive-)
            target_link_libraries(${test_target} PRIVATE delayimp)
            target_link_options(${test_target} PRIVATE
                /DELAYLOAD:avformat-63.dll /DELAYLOAD:avcodec-63.dll
                /DELAYLOAD:avutil-61.dll /DELAYLOAD:swresample-7.dll)
        endforeach()
        set_property(TARGET file_ops_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(file_ops_smoke PRIVATE /W4 /permissive-)
        set_property(TARGET copy_progress_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(copy_progress_smoke PRIVATE /W4 /permissive-)
        set_property(TARGET usb_copy_live_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(usb_copy_live_smoke PRIVATE /W4 /permissive-)
        set_property(TARGET settings_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(settings_smoke PRIVATE /W4 /permissive-)
        set_property(TARGET browser_sort_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(browser_sort_smoke PRIVATE /W4 /permissive-)
        set_property(TARGET metadata_loader_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(metadata_loader_smoke PRIVATE /W4 /permissive-)
        set_property(TARGET playlist_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(playlist_smoke PRIVATE /W4 /permissive-)
        set_property(TARGET localization_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(localization_smoke PRIVATE /W4 /permissive- /utf-8)
        set_property(TARGET ui_command_helper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(ui_command_helper PRIVATE /W4 /permissive-)
    endif()
endif()
