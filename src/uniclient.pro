QT = core gui network xml opengl multimedia
TEMPLATE = app
VERSION = 0.0.1

TARGET = uniclient
DESTDIR = ../bin

INCLUDEPATH += ../contrib/ffmpeg24894/include

RESOURCES += mainwnd.qrc
FORMS += mainwnd.ui
#LIBS += ../contrib/ffmpeg/libs



HEADERS = 
SOURCES = 
HEADERS += mainwnd.h
HEADERS += settings.h
HEADERS += ui_mainwnd.h
HEADERS += video_camera.h
HEADERS += base/adaptivesleep.h
HEADERS += base/associativearray.h
HEADERS += base/base.h
HEADERS += base/bytearray.h
HEADERS += base/expensiveobject.h
HEADERS += base/log.h
HEADERS += base/longrunnable.h
HEADERS += base/rand.h
HEADERS += base/sleep.h
HEADERS += base/threadqueue.h
HEADERS += camera/abstractrenderer.h
HEADERS += camera/audiostreamdisplay.h
HEADERS += camera/camdisplay.h
HEADERS += camera/camera.h
HEADERS += camera/gl_renderer.h
HEADERS += camera/stream_recorder.h
HEADERS += camera/videostreamdisplay.h
HEADERS += data/data.h
HEADERS += data/dataprocessor.h
HEADERS += data/mediadata.h
HEADERS += decoders/audio/abstractaudiodecoder.h
HEADERS += decoders/audio/audio_struct.h
HEADERS += decoders/audio/ffmpeg_audio.h
HEADERS += decoders/ffmpeg_dll/ffmpeg_dll.h
HEADERS += decoders/video/abstractdecoder.h
HEADERS += decoders/video/ffmpeg.h
HEADERS += decoders/video/frame_info.h
HEADERS += decoders/video/ipp_h264_decoder.h
HEADERS += device/asynch_seacher.h
HEADERS += device/device.h
HEADERS += device/device_command_processor.h
HEADERS += device/device_video_layout.h
HEADERS += device/deviceserver.h
HEADERS += device/directory_browser.h
HEADERS += device/file_device.h
HEADERS += device/network_device.h
HEADERS += device/param.h
HEADERS += device/device_managmen/device_criteria.h
HEADERS += device/device_managmen/device_manager.h
HEADERS += device_plugins/archive/abstract_archive_device.h
HEADERS += device_plugins/archive/abstract_archive_stream_reader.h
HEADERS += device_plugins/archive/archive/archive_device.h
HEADERS += device_plugins/archive/archive/archive_stream_reader.h
HEADERS += device_plugins/archive/avi_files/avi_device.h
HEADERS += device_plugins/archive/avi_files/avi_parser.h
HEADERS += device_plugins/archive/avi_files/avi_strem_reader.h
HEADERS += device_plugins/arecontvision/devices/av_device.h
HEADERS += device_plugins/arecontvision/devices/av_device_server.h
HEADERS += device_plugins/arecontvision/devices/av_panoramic.h
HEADERS += device_plugins/arecontvision/devices/av_singesensor.h
HEADERS += device_plugins/arecontvision/streamreader/av_client_pull.h
HEADERS += device_plugins/arecontvision/streamreader/cpul_httpstremreader.h
HEADERS += device_plugins/arecontvision/streamreader/cpul_tftp_stremreader.h
HEADERS += device_plugins/arecontvision/streamreader/panoramic_cpul_tftp_stremreader.h
HEADERS += device_plugins/arecontvision/tools/AVJpegHeader.h
HEADERS += device_plugins/arecontvision/tools/simple_tftp_client.h
HEADERS += device_plugins/fake/devices/fake_device.h
HEADERS += device_plugins/fake/devices/fake_device_server.h
HEADERS += device_plugins/fake/streamreader/fake_file_streamreader.h
HEADERS += network/netstate.h
HEADERS += network/nettools.h
HEADERS += network/ping.h
HEADERS += network/simple_http_client.h
HEADERS += network/socket.h
HEADERS += recorder/fake_recorder_device.h
HEADERS += recorder/recorder_device.h
HEADERS += recorder/recorder_display.h
HEADERS += statistics/statistics.h
HEADERS += streamreader/cpull_stremreader.h
HEADERS += streamreader/single_shot_file_reader.h
HEADERS += streamreader/single_shot_reader.h
HEADERS += streamreader/streamreader.h
HEADERS += ui/context_menu_helper.h
HEADERS += ui/graphicsview.h
HEADERS += ui/layout_editor_wnd.h
HEADERS += ui/layout_navigator.h
HEADERS += ui/ui_common.h
HEADERS += ui/view_drag_and_drop.h
HEADERS += ui/animation/abstract_animation.h
HEADERS += ui/animation/animated_bgr.h
HEADERS += ui/animation/animated_show.h
HEADERS += ui/animation/animation_timeline.h
HEADERS += ui/animation/item_trans.h
HEADERS += ui/animation/mouse_state.h
HEADERS += ui/animation/scene_movement.h
HEADERS += ui/animation/scene_zoom.h
HEADERS += ui/device_settings/device_settings_dlg.h
HEADERS += ui/device_settings/device_settings_tab.h
HEADERS += ui/device_settings/dlg_factory.h
HEADERS += ui/device_settings/style.h
HEADERS += ui/device_settings/widgets.h
HEADERS += ui/device_settings/plugins/arecontvision/arecont_dlg.h
HEADERS += ui/video_cam_layout/layout_content.h
HEADERS += ui/video_cam_layout/layout_items.h
HEADERS += ui/video_cam_layout/layout_manager.h
HEADERS += ui/video_cam_layout/start_screen_content.h
HEADERS += ui/video_cam_layout/videocamlayout.h
HEADERS += ui/videoitem/abstract_scene_item.h
HEADERS += ui/videoitem/custom_draw_button.h
HEADERS += ui/videoitem/img_item.h
HEADERS += ui/videoitem/layout_item.h
HEADERS += ui/videoitem/loup_wnd.h
HEADERS += ui/videoitem/recorder_item.h
HEADERS += ui/videoitem/static_image_item.h
HEADERS += ui/videoitem/video_wnd_archive_item.h
HEADERS += ui/videoitem/video_wnd_item.h
HEADERS += ui/videoitem/search/search_edit.h
HEADERS += ui/videoitem/search/search_filter_item.h
HEADERS += ui/videoitem/subitem/abstract_image_sub_item.h
HEADERS += ui/videoitem/subitem/abstract_sub_item.h
HEADERS += ui/videoitem/subitem/abstract_sub_item_container.h
HEADERS += ui/videoitem/subitem/recording_sign_item.h
HEADERS += ui/videoitem/subitem/archive_navifation/archive_navigator_item.h
HEADERS += ui/videoitem/subitem/archive_navifation/slider_item.h
HEADERS += ui/videoitem/unmoved/abstract_animated_unmoved_item.h
HEADERS += ui/videoitem/unmoved/abstract_unmoved_item.h
HEADERS += ui/videoitem/unmoved/unmoved_pixture_button.h
HEADERS += videodisplay/complicated_item.h


SOURCES += main.cpp
SOURCES += mainwnd.cpp
SOURCES += settings.cpp
SOURCES += video_camera.cpp
SOURCES += base/bytearray.cpp
SOURCES += base/log.cpp
SOURCES += camera/audiostreamdisplay.cpp
SOURCES += camera/camdisplay.cpp
SOURCES += camera/camera.cpp
SOURCES += camera/gl_renderer.cpp
SOURCES += camera/stream_recorder.cpp
SOURCES += camera/videostreamdisplay.cpp
SOURCES += data/dataprocessor.cpp
SOURCES += debug/qrc_mainwnd.cpp
SOURCES += decoders/audio/abstractaudiodecoder.cpp
SOURCES += decoders/audio/ffmpeg_audio.cpp
SOURCES += decoders/ffmpeg_dll/ffmpeg_dll.cpp
SOURCES += decoders/video/abstractdecoder.cpp
SOURCES += decoders/video/ffmpeg.cpp
SOURCES += decoders/video/frame_info.cpp
SOURCES += decoders/video/ipp_h264_decoder.cpp
SOURCES += device/asynch_seacher.cpp
SOURCES += device/device.cpp
SOURCES += device/device_command_processor.cpp
SOURCES += device/deviceserver.cpp
SOURCES += device/directory_browser.cpp
SOURCES += device/file_device.cpp
SOURCES += device/network_device.cpp
SOURCES += device/param.cpp
SOURCES += device/device_managmen/device_criteria.cpp
SOURCES += device/device_managmen/device_manager.cpp
SOURCES += device_plugins/archive/abstract_archive_device.cpp
SOURCES += device_plugins/archive/abstract_archive_stream_reader.cpp
SOURCES += device_plugins/archive/archive/archive_device.cpp
SOURCES += device_plugins/archive/archive/archive_stream_reader.cpp
SOURCES += device_plugins/archive/avi_files/avi_device.cpp
SOURCES += device_plugins/archive/avi_files/avi_parser.cpp
SOURCES += device_plugins/archive/avi_files/avi_strem_reader.cpp
SOURCES += device_plugins/arecontvision/devices/av_device.cpp
SOURCES += device_plugins/arecontvision/devices/av_device_server.cpp
SOURCES += device_plugins/arecontvision/devices/av_panoramic.cpp
SOURCES += device_plugins/arecontvision/devices/av_singesensor.cpp
SOURCES += device_plugins/arecontvision/streamreader/av_client_pull.cpp
SOURCES += device_plugins/arecontvision/streamreader/cpul_httpstremreader.cpp
SOURCES += device_plugins/arecontvision/streamreader/cpul_tftp_stremreader.cpp
SOURCES += device_plugins/arecontvision/streamreader/panoramic_cpul_tftp_stremreader.cpp
SOURCES += device_plugins/arecontvision/tools/AVJpegHeader.cpp
SOURCES += device_plugins/arecontvision/tools/nalconstructor.cpp
SOURCES += device_plugins/arecontvision/tools/simple_tftp_client.cpp
SOURCES += device_plugins/fake/devices/fake_device.cpp
SOURCES += device_plugins/fake/devices/fake_device_server.cpp
SOURCES += device_plugins/fake/streamreader/fake_file_streamreader.cpp
SOURCES += network/netstate.cpp
SOURCES += network/nettools.cpp
SOURCES += network/ping.cpp
SOURCES += network/simple_http_client.cpp
SOURCES += network/socket.cpp
SOURCES += recorder/fake_recorder_device.cpp
SOURCES += recorder/recorder_device.cpp
SOURCES += recorder/recorder_display.cpp
SOURCES += release/qrc_mainwnd.cpp
SOURCES += statistics/statistics.cpp
SOURCES += streamreader/cpull_stremreader.cpp
SOURCES += streamreader/single_shot_file_reader.cpp
SOURCES += streamreader/single_shot_reader.cpp
SOURCES += streamreader/streamreader.cpp
SOURCES += ui/context_menu_helper.cpp
SOURCES += ui/graphicsview.cpp
SOURCES += ui/layout_editor_wnd.cpp
SOURCES += ui/layout_navigator.cpp
SOURCES += ui/ui_common.cpp
SOURCES += ui/view_drag_and_drop.cpp
SOURCES += ui/animation/abstract_animation.cpp
SOURCES += ui/animation/animated_bgr.cpp
SOURCES += ui/animation/animated_show.cpp
SOURCES += ui/animation/animation_timeline.cpp
SOURCES += ui/animation/item_trans.cpp
SOURCES += ui/animation/mouse_state.cpp
SOURCES += ui/animation/scene_movement.cpp
SOURCES += ui/animation/scene_zoom.cpp
SOURCES += ui/device_settings/device_settings_dlg.cpp
SOURCES += ui/device_settings/device_settings_tab.cpp
SOURCES += ui/device_settings/dlg_factory.cpp
SOURCES += ui/device_settings/style.cpp
SOURCES += ui/device_settings/widgets.cpp
SOURCES += ui/device_settings/plugins/arecontvision/arecont_dlg.cpp
SOURCES += ui/video_cam_layout/layout_content.cpp
SOURCES += ui/video_cam_layout/layout_items.cpp
SOURCES += ui/video_cam_layout/layout_manager.cpp
SOURCES += ui/video_cam_layout/start_screen_content.cpp
SOURCES += ui/video_cam_layout/videocamlayout.cpp
SOURCES += ui/videoitem/abstract_scene_item.cpp
SOURCES += ui/videoitem/custom_draw_button.cpp
SOURCES += ui/videoitem/img_item.cpp
SOURCES += ui/videoitem/layout_item.cpp
SOURCES += ui/videoitem/loup_wnd.cpp
SOURCES += ui/videoitem/recorder_item.cpp
SOURCES += ui/videoitem/static_image_item.cpp
SOURCES += ui/videoitem/video_wnd_archive_item.cpp
SOURCES += ui/videoitem/video_wnd_item.cpp
SOURCES += ui/videoitem/search/search_edit.cpp
SOURCES += ui/videoitem/search/search_filter_item.cpp
SOURCES += ui/videoitem/subitem/abstract_image_sub_item.cpp
SOURCES += ui/videoitem/subitem/abstract_sub_item.cpp
SOURCES += ui/videoitem/subitem/recording_sign_item.cpp
SOURCES += ui/videoitem/subitem/archive_navifation/archive_navigator_item.cpp
SOURCES += ui/videoitem/subitem/archive_navifation/slider_item.cpp
SOURCES += ui/videoitem/unmoved/abstract_animated_unmoved_item.cpp
SOURCES += ui/videoitem/unmoved/abstract_unmoved_item.cpp
SOURCES += ui/videoitem/unmoved/unmoved_pixture_button.cpp
SOURCES += videodisplay/complicated_item.cpp
