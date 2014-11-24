TEMPLATE = app
CONFIG += console

CONFIG(debug,debug|release) {
  LIBS += -lgtestd -lgtest_main-mdd
} else {
  LIBS += -lgtest -lgtest_main-md
}

!win32 {
  ext_debug2.target  = $(TARGET).debug
  ext_debug2.depends = $(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(TARGET) $(TARGET).debug; $(STRIP) -g $(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(TARGET).debug $(TARGET); touch $(TARGET).debug
  ext_debug.depends = $(TARGET).debug
  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}
