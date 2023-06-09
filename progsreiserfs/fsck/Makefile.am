sbin_PROGRAMS 		= fsck.reiserfs
fsck_reiserfs_SOURCES 	= fsck.c pass0.c

fsck_reiserfs_LDADD 	= @INTLLIBS@ @LIBS@ \
			  $(top_builddir)/progsreiserfs/libmisc/libmisc.la \
	    		  $(top_builddir)/libreiserfs/libreiserfs.la \
	    		  $(top_builddir)/libdal/libdal.la
fsck_reiserfs_LDFLAGS 	= @PROGS_LDFLAGS@

INCLUDES		= -I$(top_srcdir)/include @INTLINCS@
