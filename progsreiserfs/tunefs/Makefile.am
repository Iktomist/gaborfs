sbin_PROGRAMS 		= tunefs.reiserfs
tunefs_reiserfs_SOURCES = tunefs.c

tunefs_reiserfs_LDADD 	= @INTLLIBS@ @LIBS@ \
			  $(top_builddir)/progsreiserfs/libmisc/libmisc.la \
	    		  $(top_builddir)/libreiserfs/libreiserfs.la \
	    		  $(top_builddir)/libdal/libdal.la
tunefs_reiserfs_LDFLAGS = @PROGS_LDFLAGS@
  
INCLUDES		= -I$(top_srcdir)/include @INTLINCS@
