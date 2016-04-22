AC_PREREQ([2.68])
AC_INIT([libump], [3.0], [libv@skynet.be])
AC_CONFIG_SRCDIR([src/ump_platform.h])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

LT_INIT

LIBUMP_VERSION_MAJOR=3
AC_SUBST([LIBUMP_VERSION_MAJOR])

# Support silent build rules, requires at least automake-1.11. Disable
# by either passing --disable-silent-rules to configure or passing V=1
# to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])],
    [AC_SUBST([AM_DEFAULT_VERBOSITY], [1])])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memset munmap])

AC_CHECK_HEADER([pthread.h])
AC_CHECK_LIB([pthread],[pthread_mutex_lock],[],[])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/ump.h
	libump.pc
])

AC_OUTPUT
