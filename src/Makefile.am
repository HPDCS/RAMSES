ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src 
AM_MAKEFLAGS = -s
AUTOMAKE_OPTIONS = subdir-objects

librootsim_include_HEADERS = RAMSES.h
librootsim_includedir = $(includedir)
lib_LIBRARIES = libabm.a libabm_mm.a

libabm_a_SOURCES =	api.c \
			calqueue.c \
			core.c \
			numerical.c \
			message.c \
			topology.c \
			reverse.c \
			function_map.c

libabm_mm_a_SOURCES = 	allocator.c \
			dymelor.c \
			recoverable.c

