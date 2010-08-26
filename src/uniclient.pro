QT = core gui network xml opengl
TEMPLATE = app
VERSION = 0.0.1


TARGET = uniclient

DESTDIR = ../bin


INCLUDEPATH += ../contrib

SOURCES = graphicsview.cpp main.cpp mainwnd.cpp uvideo_wnd.cpp video_camera.cpp 

SOURCES += base/log.cpp base/bytearray.cpp
SOURCES += camera/camera.cpp
SOURCES += data/dataprocessor.cpp
SOURCES += decoders/video/frame_info.cpp decoders/video/ffmpeg.cpp decoders/video/abstractdecoder.cpp decoders/video/ipp_h264_decoder.cpp
SOURCES += device/device.cpp device/network_device.cpp device/file_device.cpp device/asynch_seacher.cpp device/deviceserver.cpp device/device_command_processor.cpp device/directory_browser.cpp
SOURCES += network/nettools.cpp network/socket.cpp network/simple_http_client.cpp network/ping.cpp network/netstate.cpp
SOURCES += statistics/statistics.cpp
SOURCES += streamreader/cpull_stremreader.cpp streamreader/streamreader.cpp streamreader/single_shot_reader.cpp streamreader/single_shot_file_reader.cpp
SOURCES += videodisplay/camdisplay.cpp videodisplay/gl_renderer.cpp videodisplay/video_window.cpp videodisplay/videostreamdisplay.cpp

HEADERS = graphicsview.h mainwnd.h uvideo_wnd.h video_camera.h
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

HEADERS += videodisplay/video_cam_layout/videocamlayout.h  videodisplay/animation/animation_timeline.h videodisplay/animation/mouse_state.h
HEADERS += videodisplay/animation/scene_movement.h videodisplay/animation/scene_zoom.h videodisplay/animation/abstract_animation.h videodisplay/animation/item_trans.h

SOURCES += videodisplay/video_cam_layout/videocamlayout.cpp videodisplay/animation/animation_timeline.cpp videodisplay/animation/mouse_state.cpp
SOURCES += videodisplay/animation/scene_movement.cpp videodisplay/animation/scene_zoom.cpp videodisplay/animation/abstract_animation.cpp videodisplay/animation/item_trans.cpp

SOURCES += videodisplay/menu/menu_button.cpp videodisplay/menu/grapicsview_context_menu.cpp
HEADERS += videodisplay/menu/menu_button.h videodisplay/menu/grapicsview_context_menu.h

RESOURCES += mainwnd.qrc
FORMS += mainwnd.ui
#LIBS += ../contrib/ffmpeg/libs

