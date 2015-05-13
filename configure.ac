C_PREREQ([2.67])

AC_INIT([ABM-Sim],[1.0.0],[pellegrini@dis.uniroma1.it],[abmsim],[https://github.com/HPDCS/ABMSim])
AC_CONFIG_SRCDIR([src])
AM_INIT_AUTOMAKE([1.11 -Wall foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS




# Debugging symbols or not?
CFLAGS="-O3 -lm"
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debugging of the platform]))

AS_IF([test "x$enable_debug" = "xyes"], [
	CFLAGS="-g3 -pg -Wall -Wextra -O0 -Wswitch-enum -Wcast-align -Wpointer-arith -Wstrict-overflow=5 -Wstrict-prototypes -Winline -Wundef -Wnested-externs -Wshadow -Wunreachable-code -Wfloat-equal -Wredundant-decls -Wold-style-definition -std=c99 -fno-omit-frame-pointer -ffloat-store -fno-common -fstrict-aliasing -fgnu89-inline -rdynamic -lm"
])



#----------------------------------------------------------------------------
# Check for languages
#----------------------------------------------------------------------------
AC_LANG([C])


#----------------------------------------------------------------------------
# Checks for programs.
#----------------------------------------------------------------------------
AC_PROG_CC([gcc])
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_CC_C_O
AM_PROG_CC_C_O
LT_INIT


#----------------------------------------------------------------------------
# Checks for C header files and libraries
#----------------------------------------------------------------------------
AC_CHECK_LIB([m], [log], [true], AC_MSG_FAILURE([libm not found]))

# Check for POSIX thread support
AX_PTHREAD([], [AC_MSG_ERROR([PTHREAD support not found])])

LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AC_SUBST([LIBS])
AC_SUBST([CFLAGS])
AC_SUBST([CC])


#----------------------------------------------------------------------------
# Check scripts sizes.
# The compiler script is dynamically built in order to create ad compile
# time linker scripts which will be used to produce the final version
# of the executable. We compute here the sizes of those cat'd scripts
# in order to make the compiler behave consistently with the last version
# of the linker scripts provided in the installation.
#----------------------------------------------------------------------------
AC_MSG_CHECKING([compiler and linker scripts sizes])
AC_SUBST([cc_have_mixed_ss], [$ac_have_mixed_ss])
AC_SUBST([cc_script_lines], [$(wc -l script/abmcc.in | sed 's,\ .*,,g')])
AC_SUBST([cc_hijacker_conf_lines], [$(wc -l script/hijacker-conf.xml | sed 's,\ .*,,g')])
AC_MSG_RESULT([ok])


# Generate Makefiles for all these subfolders
#AC_OUTPUT
AC_CONFIG_FILES([Makefile script/Makefile src/Makefile])
AC_OUTPUT
