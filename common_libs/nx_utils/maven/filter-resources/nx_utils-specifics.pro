TEMPLATE = lib
CONFIG += console

win* {
    DEFINES += NX_UTILS_API=__declspec(dllexport)
}

!win32 {
  ext_debug2.target  = $(DESTDIR)$(TARGET).debug
  ext_debug2.depends = $(DESTDIR)$(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(DESTDIR)/$(TARGET) $(DESTDIR)/$(TARGET).debug; $(STRIP) -g $(DESTDIR)/$(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(DESTDIR)/$(TARGET).debug $(DESTDIR)/$(TARGET); touch $(DESTDIR)/$(TARGET).debug

  ext_debug.depends = $(DESTDIR)$(TARGET).debug

  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}
