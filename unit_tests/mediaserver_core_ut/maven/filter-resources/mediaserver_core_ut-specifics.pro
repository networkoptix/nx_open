CONFIG += console

win32 {
	DEFINES += NOMINMAX
}
INCLUDEPATH += ${root.dir}/mediaserver_core/src

LIBS += $$FESTIVAL_LIB

