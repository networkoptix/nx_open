TEMPLATE = lib

include($$ADDITIONAL_QT_INCLUDES/qtsingleapplication/src/qtsinglecoreapplication.pri)
include($$ADDITIONAL_QT_INCLUDES/qtservice/src/qtservice.pri)

INCLUDEPATH +=  ${qt.dir}/include/QtNetwork/ \
                ${qt.dir}/include/QtNetwork/$$QT_VERSION/ \
                ${qt.dir}/include/QtNetwork/$$QT_VERSION/QtNetwork/ \

exists( ${libdir}/libcreateprocess.pri ) {
  include(${libdir}/libcreateprocess.pri)
}

INCLUDEPATH += ${root.dir}/appserver2/src/
INCLUDEPATH += ${root.dir}/nx_cloud/cloud_db_client/src/include/
INCLUDEPATH += ${root.dir}/common_libs/nx_email/src/
INCLUDEPATH += ${root.dir}/common_libs/nx_network/src/

!win32 {
  ext_debug2.target  = $(DESTDIR)$(TARGET).debug
  ext_debug2.depends = $(DESTDIR)$(TARGET)
  ext_debug2.commands = $$QMAKE_OBJCOPY --only-keep-debug $(DESTDIR)/$(TARGET) $(DESTDIR)/$(TARGET).debug; $(STRIP) -g $(DESTDIR)/$(TARGET); $$QMAKE_OBJCOPY --add-gnu-debuglink=$(DESTDIR)/$(TARGET).debug $(DESTDIR)/$(TARGET); touch $(DESTDIR)/$(TARGET).debug

  ext_debug.depends = $(DESTDIR)$(TARGET).debug

  QMAKE_EXTRA_TARGETS += ext_debug ext_debug2
}

