sbin_PROGRAMS 			= resizefs.reiserfs
resizefs_reiserfs_SOURCES 	= resizefs.c

resizefs_reiserfs_LDADD 	= @INTLLIBS@ @LIBS@ \
				  $(top_builddir)/progsreiserfs/libmisc/libmisc.la \
	    			  $(top_builddir)/libreiserfs/libreiserfs.la \
	    			  $(top_builddir)/libdal/libdal.la
resizefs_reiserfs_LDFLAGS 	= @PROGS_LDFLAGS@

INCLUDES			= -I$(top_srcdir)/include @INTLINCS@
