ifndef ARCH
ARCH		= linux
endif

ifeq ($(ARCH),alpha)

FLAGS		= -g3 -O2 -DIW_DEBUG
ICEWING		= /vol/vita/bin/icewing-config

else

FLAGS		= -g -O2 -fPIC -DIW_DEBUG
USE_GCC		= USE_GCC
ICEWING		= $(shell which icewing-config 1>/dev/null 2>&1 && \
			echo icewing-config || echo /vol/vita/bin/icewing-config)

endif

ICEWING_CFLAGS	:= $(shell $(ICEWING) --cflags)

CFLAGS		+= $(FLAGS) $(ICEWING_CFLAGS)
CXXFLAGS	+= $(FLAGS) $(ICEWING_CFLAGS)
LDFLAGS		+= $(FLAGS)
LDLIBS		+= `$(ICEWING) --libs`

SRCS = shmdata.c

OBJS += $(patsubst %c,%o,$(filter %.c,$(SRCS))) \
	$(patsubst %C,%o,$(filter %.C,$(SRCS))) \
	$(patsubst %cpp,%o,$(filter %.cpp,$(SRCS)))

LIBMAJOR	= 0
LIBRARY		= libshmdata.so

all: ${LIBRARY}

######################################################################

ifdef USE_GCC

CC	= gcc
CXX	= g++
LD	= gcc
FLAGS	+= -Wall

else

CC	= cc
CXX	= cxx
LD	= ld
FLAGS	+= -w0 -std1
CXXFLAGS+= -msg_disable undpreid -msg_disable declbutnotref

endif

ifeq ($(ARCH),alpha)
LINK_SHARED	= ${LD} -o ${1} -shared -soname ${1} -set_version ${2} \
		-expect_unresolved \* -B symbolic $^ ${LDLIBS}
endif

ifeq ($(ARCH),sol2)
LINK_SHARED	= ${LD} -o ${1} -G -z allextract -B symbolic \
		-h ${1} ${LDFLAGS} $^ ${LDLIBS}
endif

ifeq ($(ARCH),macosx)
LINK_SHARED	= ${LD} -Wl,-search_paths_first -o ${1} -bundle -undefined dynamic_lookup \
		${LDFLAGS} $^ ${LDLIBS}
endif

ifeq ($(ARCH),linux)
LINK_SHARED	= ${LD} -o ${1} -shared -Wl,-soname,${1} ${LDFLAGS} $^ ${LDLIBS}
endif

######################################################################

${LIBRARY}: $(OBJS)
	$(call LINK_SHARED,${LIBRARY},${LIBMAJOR})

install: $(LIBRARY)
	install -m 444 $^ `$(ICEWING) --libdir`

############ MAINTAINANCE ############################################

.PHONY: clean depend pack

pack:
	tar --use-compress-program bzip2 -cvf shmdata.tar.bz2 \
		Makefile *.c

depend:
	gcc -MM $(filter-out -I/usr/include%,$(CFLAGS)) $(SRCS) 2>/dev/null | sed 's/[^:]*: \([^.]*\)/\1.o: \1/' >.depend

clean:
	echo "Removing all generated files..."
	rm -f $(OBJS) ${LIBRARY}

-include .depend
