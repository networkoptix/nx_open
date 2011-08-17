QT *= opengl

HEADERS += camera/abstractrenderer.h \
        camera/audiostreamdisplay.h \
        camera/camdisplay.h \
        camera/gl_renderer.h \
        camera/videostreamdisplay.h \
        camera/yuvconvert.h

SOURCES += camera/audiostreamdisplay.cpp \
        camera/camdisplay.cpp \
        camera/gl_renderer.cpp \
        camera/videostreamdisplay.cpp \
        camera/yuvconvert.cpp

HEADERS += videoitem/abstract_scene_item.h \
        videoitem/abstract_sub_item.h \
        videoitem/img_item.h \
        videoitem/video_wnd_item.h

SOURCES += videoitem/abstract_scene_item.cpp \
        videoitem/abstract_sub_item.cpp \
        videoitem/img_item.cpp \
        videoitem/video_wnd_item.cpp
