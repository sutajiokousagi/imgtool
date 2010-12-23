# $Id$
#
# Source makefile for imgtool
#
# Handles capture to jpeg and png as well as display of jpeg and png images
#

DEBUG=0
MACHINE:=$(shell uname -m)
ifneq (${MACHINE},x86_64)
MACHINE:=x86
endif
HOST_TARGET=${MACHINE}-linux

ifneq ($(TARGET),)
  TARGETDIR=$(TARGET)
  ifneq (${TARGET},${HOST_TARGET})
	TGT = $(addsuffix -,$(TARGET))
  endif
endif

ifeq ($(CNPLATFORM),)
  CNPLATFORM=unknown
endif

CC=$(TGT)gcc
AR=$(TGT)ar
RANLIB=$(TGT)ranlib
LN=$(TGT)ln
STRIP=$(TGT)strip

ifeq ($(DEBUG),1)
OPTFLAGS=
else
OPTFLAGS= -Os
endif
ifeq (${TARGET},${HOST_TARGET})
ADD_LIB_FLAGS=
ADD_C_FLAGS= -DNO_PNG
else
ADD_LIB_FLAGS= -lpng
ADD_C_FLAGS=
endif
FLAGS= -g $(OPTFLAGS) -fno-rtti -fconserve-space -fno-exceptions -I../../../imports/libs/all/all/include  -DCNPLATFORM_$(CNPLATFORM) -DCNPLATFORM=\"$(CNPLATFORM)\" ${ADD_C_FLAGS}
LDFLAGS= -lz ${ADD_LIB_FLAGS} -ljpeg -L../../../imports/libs/$(TARGET)/lib

RM=rm -f

SOURCES=$(wildcard *.cpp)
BINARIES=imgtool
SRC_BINARIES=$(addprefix ${CNPLATFORM}-${TARGET}/,${BINARIES})
SRC86_BINARIES=$(addprefix ${CNPLATFORM}-${HOST_TARGET}/,${BINARIES})
OBJS=$(patsubst %.cpp,%.o,${SOURCES})
SRC_OBJS=$(addprefix ${CNPLATFORM}-${TARGET}/,${OBJS})
EXPORT_BINARIES=$(addprefix $(PREFIX)/usr/bin/,$(BINARIES))
ifneq (${TARGET},${HOST_TARGET})
EXPORT86_BINARIES=$(addprefix ${PREFIX}/${MACHINE}-bin/,${BINARIES})
endif

all : ${SRC_BINARIES}

${CNPLATFORM}-${TARGET}/imgtool: ${CNPLATFORM}-${TARGET} ${SRC_OBJS}
	$(CC) -o $@ $(FLAGS) ${SRC_OBJS} $(LDFLAGS)
	cp $@ $@.debug
	$(STRIP) $@

${CNPLATFORM}-${TARGET}:
	mkdir -p $@

${SRC_OBJS} : ${CNPLATFORM}-${TARGET}/%.o : %.cpp
	${CC} -o $@ -c ${FLAGS} $<

$(EXPORT_BINARIES): ${SRC_BINARIES}
	install -p -D $? $@

ifneq (${TARGET},${HOST_TARGET})
${EXPORT86_BINARIES} : ${SRC86_BINARIES}
	install -p -D $? $@

${CNPLATFORM}-${HOST_TARGET}/imgtool: ${SOURCES}
	${MAKE} TARGET=${HOST_TARGET}

else
EXPORT86_BINARIES=
endif

install: ${EXPORT_BINARIES} ${EXPORT86_BINARIES}

clean :
	$(RM) *.o *.debug ${BINARIES} ${CNPLATFORM}/*.o ${CNPLATFORM}-${TARGET}/* ${CNPLATFORM}-${HOST_TARGET}/* ${SRC_BINARIES}

distclean : clean
	$(RM) $(EXPORT_BINARIES)

.PHONY: exports clean all copy-exports

