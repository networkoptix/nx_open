TEMPLATE = app
CONFIG += console

include( ${libdir}/gtest.pri )

!win32 {
  ext_debug2.target  = $(TARGET).debug
  ext_debug2.depends = $(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(TARGET) $(TARGET).debug; $(STRIP) -g $(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(TARGET).debug $(TARGET); touch $(TARGET).debug
  ext_debug.depends = $(TARGET).debug
  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}
