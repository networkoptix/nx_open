lib_LTLIBRARIES = libUMP.la
libUMP_la_LDFLAGS = -version-info @LIBUMP_VERSION_MAJOR@
libUMP_la_LIBADD = @LIBS@
libUMP_la_SOURCES = \
	ump_arch.c \
	ump_frontend.c \
	ump_osu_locks.c \
	ump_osu_memory.c \
	ump_ref_drv.c \
	ump_uku.c

library_includedir=$(includedir)/ump/
library_include_HEADERS = ump.h ump_platform.h ump_ref_drv.h
