TEMPLATE = app
CONFIG += console

exists( ${libdir}/libcreateprocess.pri ) {
  include(${libdir}/libcreateprocess.pri)
}
exists( ${libdir}/sasl2.pri ) {
  include(${libdir}/sasl2.pri)
}
exists( ${libdir}/openldap.pri ) {
  include(${libdir}/openldap.pri)
}

INCLUDEPATH += ${root.dir}/appserver2/src/
INCLUDEPATH += ${root.dir}/mediaserver_core/src/

!win32 {
  ext_debug2.target  = $(TARGET).debug
  ext_debug2.depends = $(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(TARGET) $(TARGET).debug; $(STRIP) -g $(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(TARGET).debug $(TARGET); touch $(TARGET).debug
  ext_debug.depends = $(TARGET).debug
  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}
