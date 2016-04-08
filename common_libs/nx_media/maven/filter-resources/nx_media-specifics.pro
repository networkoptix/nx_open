TEMPLATE = lib
CONFIG += console

INCLUDEPATH += ${root.dir}/common/src/ \
               ${qt.dir}/include/QtCore/ \
               ${qt.dir}/include/QtCore/$$QT_VERSION/ \
               ${qt.dir}/include/QtCore/$$QT_VERSION/QtCore/ \
               ${qt.dir}/include/QtGui/ \
               ${qt.dir}/include/QtGui/$$QT_VERSION/ \
               ${qt.dir}/include/QtGui/$$QT_VERSION/QtGui/ \
               ${qt.dir}/include/QtMultimedia/ \
               ${qt.dir}/include/QtMultimedia/$$QT_VERSION/ \
               ${qt.dir}/include/QtMultimedia/$$QT_VERSION/QtMultimedia/ \
               ${qt.dir}/include/QtOpenGL/ \
               ${qt.dir}/include/QtOpenGL/$$QT_VERSION/ \
               ${qt.dir}/include/QtOpenGL/$$QT_VERSION/QtOpenGL/ \
               ${root.dir}/common_libs/nx_media/proxy_decoder/ \

LIBS += -L${root.dir}/common_libs/nx_media/proxy_decoder -lproxydecoder

!win32 {
  ext_debug2.target  = $(DESTDIR)$(TARGET).debug
  ext_debug2.depends = $(DESTDIR)$(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(DESTDIR)/$(TARGET) $(DESTDIR)/$(TARGET).debug; $(STRIP) -g $(DESTDIR)/$(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(DESTDIR)/$(TARGET).debug $(DESTDIR)/$(TARGET); touch $(DESTDIR)/$(TARGET).debug

  ext_debug.depends = $(DESTDIR)$(TARGET).debug

  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}

android {
    QT += androidextras
}
