# $Id$
# imgtool wrapper makefile
#

EXTRA_LIBS=
EXTRA_INCLUDES=
EXTRA_LIBS_PATH:=$(shell echo "$(EXTRA_LIBS)" | sed 's/[ ]\+/:/g')

all: build

build: config
	$(MAKE) -C src

config:
	@echo "====[ Configuration completed ]===="

clean:
	$(MAKE) -C src clean

install: build
	@echo "Installing to $(PREFIX)"
	$(MAKE) -C src install

# config should NOT be phony
.PHONY: all clean install build

