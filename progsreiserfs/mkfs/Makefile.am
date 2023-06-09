sbin_PROGRAMS 		= mkfs.reiserfs
mkfs_reiserfs_SOURCES 	= mkfs.c

mkfs_reiserfs_LDADD 	= @INTLLIBS@ @LIBS@ @UUID_LIBS@\
			  $(top_builddir)/progsreiserfs/libmisc/libmisc.la \
	    		  $(top_builddir)/libreiserfs/libreiserfs.la \
	    		  $(top_builddir)/libdal/libdal.la
mkfs_reiserfs_LDFLAGS 	= @PROGS_LDFLAGS@
  
INCLUDES		= -I$(top_srcdir)/include @INTLINCS@
