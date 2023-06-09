SUBDIRS = intl po libdal libreiserfs progsreiserfs demos include doc debug

EXTRA_DIST = ABOUT-NLS AUTHORS BUGS COPYING TODO NEWS THANKS README \
	progsreiserfs.spec.in progsreiserfs.m4 ltconfig

aclocaldir = $(datadir)/aclocal
aclocal_DATA = progsreiserfs.m4

dist-hook: progsreiserfs.spec
	cp progsreiserfs.spec $(distdir)

