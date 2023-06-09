sbin_PROGRAMS 		= cpfs.reiserfs
cpfs_reiserfs_SOURCES 	= cpfs.c

cpfs_reiserfs_LDADD 	= @INTLLIBS@ @LIBS@ \
			  $(top_builddir)/progsreiserfs/libmisc/libmisc.la \
	    		  $(top_builddir)/libreiserfs/libreiserfs.la \
	    		  $(top_builddir)/libdal/libdal.la
cpfs_reiserfs_LDFLAGS 	= @PROGS_LDFLAGS@
  
INCLUDES		= -I$(top_srcdir)/include @INTLINCS@
