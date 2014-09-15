TEMPLATE = lib
QT += core gui network xml sql concurrent multimedia

mac {
  OBJECTIVE_SOURCES += ${basedir}/src/utils/mac_utils.mm
  LIBS += -lobjc -framework Foundation -framework AppKit
}

TRANSLATIONS += ${basedir}/translations/common_en.ts \

#               ${basedir}/translations/common_zh-CN.ts \
#               ${basedir}/translations/common_fr.ts \
#               ${basedir}/translations/common_jp.ts \
#               ${basedir}/translations/qt_ru.ts \
#               ${basedir}/translations/qt_zh-CN.ts \
#               ${basedir}/translations/qt_fr.ts \
#               ${basedir}/translations/qt_jp.ts \
#               ${basedir}/translations/qt_ko.ts \
#               ${basedir}/translations/qt_pt-BR.ts \

#               ${basedir}/translations/common_ru.ts \
#               ${basedir}/translations/common_ko.ts \
#               ${basedir}/translations/common_pt-BR.ts \


!win32 {
  ext_debug2.target  = $(DESTDIR)$(TARGET).debug
  ext_debug2.depends = $(DESTDIR)$(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(DESTDIR)/$(TARGET) $(DESTDIR)/$(TARGET).debug; $(STRIP) -g $(DESTDIR)/$(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(DESTDIR)/$(TARGET).debug $(DESTDIR)/$(TARGET); touch $(DESTDIR)/$(TARGET).debug

  ext_debug.depends = $(DESTDIR)$(TARGET).debug

  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}

SOURCES += ${basedir}/${arch}/app_info_impl.cpp
