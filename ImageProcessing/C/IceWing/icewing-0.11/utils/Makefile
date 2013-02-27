#WITH_DACS	= WITH_DACS
#WITH_XCF	= WITH_XCF

ICEWING = $(shell which icewing-config 1>/dev/null 2>&1 && \
		echo icewing-config || echo /vol/vita/bin/icewing-config)

LDLIBS	+= -lreadline

all: icewing-control

ifdef WITH_XCF
CXXFLAGS	+= -DWITH_XCF `pkg-config --cflags xcf`
LDLIBS		+= `pkg-config --libs xcf`
endif

ifdef WITH_DACS
CXXFLAGS	+= -DWITH_DACS
include $(D3)/share/mk/general.mk
endif

install: icewing-control
	install -m 555 $^ `$(ICEWING) --bindir`
