# name of your binary
bin_PROGRAMS = nvr-app

# list of source files
# the prefix is the name of the binary
nvr_app_SOURCES = main.c nvr-app.c

# list of headers we're not going to install
noinst_HEADERS = nvr-app.h

# our CFLAGS and LDFLAGS used for compiling and linking
# make sure you prefix these with the name of your binary
nvr_app_CFLAGS = $(GST_CFLAGS)
nvr_app_LDFLAGS = $(GST_LIBS)