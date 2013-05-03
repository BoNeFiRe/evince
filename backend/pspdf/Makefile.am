INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libdocument				\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DEVINCE_COMPILATION			\
	$(BACKEND_CFLAGS)					\
	$(POPPLER_CFLAGS)                       \
	$(WARN_CXXFLAGS)			\
	$(DISABLE_DEPRECATED)

# noinst_LTLIBRARIES = libpspdfdocument.la
backend_LTLIBRARIES = libpspdfdocument.la

libpspdfdocument_la_SOURCES =		\
	pspdf-document.c		\
	pspdf-document.h		\
	pspdf-converter.c		\
	pspdf-converter.h

libpspdfdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libpspdfdocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(POPPLER_LIBS)					\
	$(CAIRO_PDF_LIBS)				\
	$(CAIRO_PS_LIBS)

backend_in_files = pspdfdocument.evince-backend.in
backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)

@EV_INTLTOOL_EVINCE_BACKEND_RULE@

-include $(top_srcdir)/git.mk
