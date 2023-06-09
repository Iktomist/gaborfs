LIBS			= @INTLLIBS@ @LIBS@

includedir 		= -I$(top_srcdir)/include

lib_LTLIBRARIES	     	= libreiserfs.la
libreiserfs_la_LDFLAGS 	= -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
			  -release $(LT_RELEASE)

libreiserfs_la_LIBADD  	= $(top_builddir)/libdal/libdal.la

libreiserfs_la_SOURCES  = libreiserfs.c debug.c gauge.c exception.c \
			  core.c bitmap.c block.c tools.c journal.c \
			  hashes.c key.c tree.c object.c segment.c \
			  path.c dir.c file.c callback.c strsep.c

INCLUDES = $(includedir) @INTLINCS@

