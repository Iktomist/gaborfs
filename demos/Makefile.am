noinst_PROGRAMS  = 	traverse lookup ls cat

traverse_SOURCES = 	traverse.c
traverse_LDADD 	 = 	$(top_builddir)/libreiserfs/libreiserfs.la \
			$(top_builddir)/libdal/libdal.la \
			$(top_builddir)/progsreiserfs/libmisc/libmisc.la
   
traverse_LDFLAGS = 	@PROGS_LDFLAGS@

lookup_SOURCES 	 = 	lookup.c
lookup_LDADD 	 = 	$(top_builddir)/libreiserfs/libreiserfs.la \
			$(top_builddir)/libdal/libdal.la \
			$(top_builddir)/progsreiserfs/libmisc/libmisc.la
   
lookup_LDFLAGS 	 = 	@PROGS_LDFLAGS@

ls_SOURCES 	 = 	ls.c
ls_LDADD 	 = 	$(top_builddir)/libreiserfs/libreiserfs.la \
			$(top_builddir)/libdal/libdal.la \
			$(top_builddir)/progsreiserfs/libmisc/libmisc.la
   
ls_LDFLAGS 	 = 	@PROGS_LDFLAGS@
  
cat_SOURCES 	 = 	cat.c
cat_LDADD 	 = 	$(top_builddir)/libreiserfs/libreiserfs.la \
			$(top_builddir)/libdal/libdal.la \
			$(top_builddir)/progsreiserfs/libmisc/libmisc.la
   
cat_LDFLAGS 	 = 	@PROGS_LDFLAGS@
  
INCLUDES	  = 	-I$(top_srcdir)/include

