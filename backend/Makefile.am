SUBDIRS = 

# Backends

if ENABLE_PDF
SUBDIRS += pdf
endif

if ENABLE_PS 
if WITH_PS_BACKEND
SUBDIRS += ps
endif

if WITH_PSPDF_BACKEND
SUBDIRS += pspdf
endif
endif

if ENABLE_DJVU
SUBDIRS += djvu
endif

if ENABLE_TIFF
SUBDIRS += tiff
endif

if ENABLE_DVI
SUBDIRS += dvi
endif

if ENABLE_COMICS
     SUBDIRS += comics
endif

if ENABLE_XPS
SUBDIRS += xps
endif

EXTRA_DIST = \
	backend.symbols

-include $(top_srcdir)/git.mk
