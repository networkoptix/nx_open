QT = core gui network xml opengl
TEMPLATE = app
VERSION = 0.0.1


TARGET = uniclient

DESTDIR = ../bin


INCLUDEPATH += ../contrib

SOURCES = main.cpp mainwnd.cpp video_camera.cpp settings.cpp

SOURCES += base/log.cpp base/bytearray.cpp
SOURCES += camera/camera.cpp
SOURCES += data/dataprocessor.cpp
SOURCES += decoders/video/frame_info.cpp decoders/video/ffmpeg.cpp decoders/video/abstractdecoder.cpp decoders/video/ipp_h264_decoder.cpp
SOURCES += device/device.cpp device/network_device.cpp device/file_device.cpp device/asynch_seacher.cpp device/deviceserver.cpp device/param.cpp device/device_command_processor.cpp device/directory_browser.cpp
SOURCES += network/nettools.cpp network/socket.cpp network/simple_http_client.cpp network/ping.cpp network/netstate.cpp
SOURCES += statistics/statistics.cpp
SOURCES += streamreader/cpull_stremreader.cpp streamreader/streamreader.cpp streamreader/single_shot_reader.cpp streamreader/single_shot_file_reader.cpp
SOURCES += videodisplay/camdisplay.cpp videodisplay/gl_renderer.cpp videodisplay/video_window.cpp videodisplay/videostreamdisplay.cpp

HEADERS = mainwnd.h video_camera.h settings.h
HEADERS += base/threadqueue.h base/log.h base/associativearray.h base/expensiveobject.h base/bytearray.h base/longrunnable.h base/rand.h base/adaptivesleep.h base/base.h base/sleep.h
HEADERS += camera/camera.h
HEADERS += data/dataprocessor.h data/data.h data/mediadata.h
HEADERS += decoders/video/frame_info.h decoders/video/ffmpeg.h decoders/video/ipp_h264_decoder.h decoders/video/abstractdecoder.h
HEADERS += device/deviceserver.h device/device.h device/network_device.h device/file_device.h device/asynch_seacher.h device/param.h device/device_command_processor.h device/directory_browser.h device/device_video_layout.h
HEADERS += network/simple_http_client.h network/netstate.h network/ping.h network/socket.h network/nettools.h
HEADERS += statistics/statistics.h
HEADERS += streamreader/cpull_stremreader.h streamreader/streamreader.h streamreader/single_shot_reader.h streamreader/single_shot_file_reader.h
HEADERS += videodisplay/abstractrenderer.h videodisplay/camdisplay.h videodisplay/gl_renderer.h videodisplay/video_window.h videodisplay/videostreamdisplay.h



SOURCES += device_plugins/arecontvision/devices/av_device.cpp device_plugins/arecontvision/devices/av_device_server.cpp device_plugins/arecontvision/devices/av_panoramic.cpp device_plugins/arecontvision/devices/av_singesensor.cpp
SOURCES += device_plugins/arecontvision/streamreader/cpul_file_test.cpp device_plugins/arecontvision/streamreader/cpul_httpstremreader.cpp device_plugins/arecontvision/streamreader/cpul_tftp_stremreader.cpp device_plugins/arecontvision/streamreader/av_client_pull.cpp
SOURCES += device_plugins/arecontvision/tools/AVJpegHeader.cpp device_plugins/arecontvision/tools/nalconstructor.cpp device_plugins/arecontvision/tools/simple_tftp_client.cpp 
SOURCES += device_plugins/arecontvision/streamreader/panoramic_cpul_tftp_stremreader.cpp

HEADERS += device_plugins/arecontvision/devices/av_device.h device_plugins/arecontvision/devices/av_device_server.h device_plugins/arecontvision/devices/av_panoramic.h device_plugins/arecontvision/devices/av_singesensor.h
HEADERS += device_plugins/arecontvision/streamreader/cpul_file_test.h device_plugins/arecontvision/streamreader/cpul_httpstremreader.h device_plugins/arecontvision/streamreader/cpul_tftp_stremreader.h device_plugins/arecontvision/streamreader/av_client_pull.h
HEADERS += device_plugins/arecontvision/tools/AVJpegHeader.h device_plugins/arecontvision/tools/simple_tftp_client.h
HEADERS += device_plugins/arecontvision/streamreader/panoramic_cpul_tftp_stremreader.h

SOURCES += device_plugins/fake/devices/fake_device_server.cpp device_plugins/fake/devices/fake_device.cpp
SOURCES += device_plugins/fake/streamreader/fake_file_streamreader.cpp

HEADERS += device_plugins/fake/devices/fake_device_server.h device_plugins/fake/devices/fake_device.h
HEADERS += device_plugins/fake/streamreader/fake_file_streamreader.h

SOURCES +=ui/graphicsview.cpp ui/videoitem/uvideo_wnd.cpp ui/videoitem/loup_wnd.cpp
HEADERS +=ui/graphicsview.h ui/videoitem/uvideo_wnd.h ui/videoitem/loup_wnd.h

HEADERS += ui/video_cam_layout/videocamlayout.h  ui/animation/animation_timeline.h ui/animation/mouse_state.h ui/animation/animated_show.h
HEADERS += ui/animation/scene_movement.h ui/animation/scene_zoom.h ui/animation/abstract_animation.h ui/animation/item_trans.h ui/animation/animated_bgr.h

SOURCES += ui/video_cam_layout/videocamlayout.cpp ui/animation/animation_timeline.cpp ui/animation/mouse_state.cpp ui/animation/animated_show.cpp
SOURCES += ui/animation/scene_movement.cpp ui/animation/scene_zoom.cpp ui/animation/abstract_animation.cpp ui/animation/item_trans.cpp ui/animation/animated_bgr.cpp 


SOURCES += ui/device_settings/device_settings_dlg.cpp ui/device_settings/device_settings_tab.cpp ui/device_settings/widgets.cpp ui/device_settings/style.cpp ui/device_settings/dlg_factory.cpp
HEADERS += ui/device_settings/device_settings_dlg.h ui/device_settings/device_settings_tab.h ui/device_settings/widgets.h ui/device_settings/style.h ui/device_settings/dlg_factory.h

SOURCES += ui/device_settings/plugins/arecontvision/arecont_dlg.cpp
HEADERS += ui/device_settings/plugins/arecontvision/arecont_dlg.h

RESOURCES += mainwnd.qrc
FORMS += mainwnd.ui
#LIBS += ../contrib/ffmpeg/libs

