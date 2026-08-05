# qrq Makefile -- Fabian Kurz, DJ5CW -- https://fkurz.net/ham/qrq.html

VERSION?=0.3.6
PREFIX?=/usr
DESTDIR?=

# set to YES if you want to use Core Audio
# note that you must use Core Audio for OSX
USE_CA=NO

# set to YES if you want to use PulseAudio instead of OSS
USE_PA=YES

# set to YES if you compile with MINGW32
USE_WIN32=NO

# set to YES if building on OSX
OSX_PLATFORM=NO

# set to YES if you want make install to build an OSX bundle instead of
# installing to PREFIX
# also directs qrq to look in bundle location for shared resources
OSX_BUNDLE=YES

ifneq ($(OSX_PLATFORM), YES)
		OSX_BUNDLE=NO
endif

ifeq ($(USE_WIN32), YES)
	CC=i686-w64-mingw32-gcc-posix
else
	CC=gcc
endif

# Set these when PDCurses is not found by the Windows compiler by default.
WIN32_CURSES_CFLAGS ?=
WIN32_CURSES_LIBS ?= -lpdcurses

INSTALL ?= install
PKG_CONFIG ?= pkg-config
TEST_CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror
WINDRES ?= windres

NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncurses 2>/dev/null)
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncurses 2>/dev/null)
PULSE_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpulse-simple 2>/dev/null)
PULSE_LIBS := $(shell $(PKG_CONFIG) --libs libpulse-simple 2>/dev/null)

ifeq ($(strip $(NCURSES_LIBS)),)
	NCURSES_LIBS = -lncurses
endif
ifeq ($(strip $(PULSE_LIBS)),)
	PULSE_LIBS = -lpulse-simple -lpulse
endif

# build information may be added by CI scripts and will be
# displayed in the help text (qrq -h)

BUILD_INFO=" Date: $(shell if [ -r ../ChangeLog ]; then date -r ../ChangeLog +%Y-%m-%dT%H:%M:%S%z; else date -r ChangeLog +%Y-%m-%dT%H:%M:%S%z; fi)\n git branch/commit: $(shell if [ x${CI_COMMIT_REF_NAME} != x ]; then echo ${CI_COMMIT_REF_NAME} ; elif [ -e .git ] || [ -e ../.git ]; then git symbolic-ref --short HEAD ; else echo not built from git repository ; fi) / $(shell if [ x${CI_COMMIT_SHA} != x ]; then echo ${CI_COMMIT_SHA}; elif [ -e .git ] || [ -e ../.git ] ; then git rev-parse --verify HEAD ; else echo - ; fi )\n by $(USER)@$(shell hostname -f)\n OS: $(shell uname -s -r -p)\n Compiler: $(shell $(CC) --version | head -1)"

# Keep backend definitions when callers add warning, sanitizer, or packaging flags.
override CFLAGS += -D PREFIX=\"$(PREFIX)\" -D BUILD_INFO=\"$(BUILD_INFO)\" -D VERSION=\"$(VERSION)\" -I.

ifeq ($(USE_CA), YES)
		CC=clang
		OBJECTS=qrq.o score.o callbase.o practice.o history.o config.o coreaudio.o
		override CFLAGS += -D CA -std=c99 -pthread
		ifeq ($(OSX_PLATFORM), YES)
		override LDFLAGS += -framework AudioUnit -framework CoreServices  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -mmacosx-version-min=10.5 -lcurses
			override CFLAGS += -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -mmacosx-version-min=10.5
			ifeq ($(OSX_BUNDLE), YES)
				CFLAGS:=$(CFLAGS) -D OSX_BUNDLE
			endif
		else  # build for iphone/ipad
			LDFLAGS:=$(LDFLAGS) -L iOSExtras/lib -framework AudioToolbox -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk
			CFLAGS:=$(CFLAGS) -I iOSExtras/include -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk
			CC:=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/gcc-4.2 -arch armv6
			IPHONE_HOST=root@localhost
			SCP=scp -P2222
			SSH=ssh -p2222
		endif
else ifeq ($(USE_PA), YES)
		override CFLAGS += -D PA -pthread $(NCURSES_CFLAGS) $(PULSE_CFLAGS)
		override LDFLAGS += -lpthread $(PULSE_LIBS) $(NCURSES_LIBS)
		OBJECTS=qrq.o score.o callbase.o practice.o history.o config.o pulseaudio.o
else ifeq ($(USE_WIN32), YES)
		override CFLAGS += -D WIN32 $(WIN32_CURSES_CFLAGS)
		override LDFLAGS += -lwinmm -lpthread $(WIN32_CURSES_LIBS)
		OBJECTS=qrq.o score.o callbase.o practice.o history.o config.o qrq-resource.o
else
		OBJECTS=qrq.o score.o callbase.o practice.o history.o config.o oss.o
		override LDFLAGS += -lpthread $(NCURSES_LIBS)
		override CFLAGS += -D OSS $(NCURSES_CFLAGS)
endif

all: qrq

qrq: $(OBJECTS)
	$(CC) -Wall -o $@ $^ -lm $(LDFLAGS)
	
.c.o:
	$(CC) -Wall $(CPPFLAGS) $(CFLAGS) -c $<

qrq-resource.o: qrq.rc qrq.ico
	$(WINDRES) -i qrq.rc -O coff -o $@

#.cpp.o:
#	g++ $(CFLAGS) -c $<

ifeq ($(OSX_BUNDLE), YES)

install: qrq
	${INSTALL} -d -v							qrq.app/Contents
	${INSTALL} -d -v							qrq.app/Contents/MacOS
	${INSTALL} -d -v							qrq.app/Contents/Resources
	${INSTALL} -d -v							qrq.app/Contents/Resources/share/qrq/
	${INSTALL} -d -v							qrq.app/Contents/Resources/share/man/man1/
	${INSTALL}    -m 0755 qrq					qrq.app/Contents/MacOS/
	${INSTALL}    -m 0755 qrqscore				qrq.app/Contents/MacOS/
	${INSTALL}    -m 0644 qrq.1					qrq.app/Contents/Resources/share/man/man1/
	${INSTALL}    -m 0644 callbase.qcb			qrq.app/Contents/Resources/share/qrq/
	${INSTALL}    -m 0644 english.qcb			qrq.app/Contents/Resources/share/qrq/
	${INSTALL}    -m 0644 cwops.qcb				qrq.app/Contents/Resources/share/qrq/
	${INSTALL}    -m 0644 qrqrc					qrq.app/Contents/Resources/share/qrq/
	${INSTALL}    -m 0644 toplist				qrq.app/Contents/Resources/share/qrq/
	${INSTALL}    -m 0644 OSXExtras/qrq.icns	qrq.app/Contents/Resources/
	${INSTALL}    -m 0755 OSXExtras/qrqsh		qrq.app/Contents/MacOS/
	sed 's/VERSION/$(VERSION)/' OSXExtras/Info.plist > qrq.app/Contents/Info.plist

uninstall:
	rm -rf qrq.app

else

install: qrq
	${INSTALL} -d -v                      $(DESTDIR)$(PREFIX)/share/qrq/
	${INSTALL} -d -v                      $(DESTDIR)$(PREFIX)/share/man/man1/
	${INSTALL} -d -v                      $(DESTDIR)$(PREFIX)/bin/
	${INSTALL} -s -m 0755 qrq             $(DESTDIR)$(PREFIX)/bin/
	${INSTALL}    -m 0755 qrqscore        $(DESTDIR)$(PREFIX)/bin/
	${INSTALL}    -m 0644 qrq.1           $(DESTDIR)$(PREFIX)/share/man/man1/
	${INSTALL}    -m 0644 callbase.qcb    $(DESTDIR)$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 english.qcb     $(DESTDIR)$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 cwops.qcb       $(DESTDIR)$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 qrqrc           $(DESTDIR)$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 toplist         $(DESTDIR)$(PREFIX)/share/qrq/

	
uninstall:
	rm -f $(PREFIX)/bin/qrq
	rm -f $(PREFIX)/bin/qrqscore
	rm -f $(PREFIX)/share/man/man1/qrq.1
	rm -f $(PREFIX)/share/qrq/callbase.qcb
	rm -f $(PREFIX)/share/qrq/english.qcb
	rm -f $(PREFIX)/share/qrq/cwops.qcb
	rm -f $(PREFIX)/share/qrq/qrqrc
	rm -f $(PREFIX)/share/qrq/toplist
	rmdir $(PREFIX)/share/qrq/

endif

package: qrq
	export CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate; ldid -s qrq
	rm -rf qrq-pkg
	${INSTALL} -d -v                      qrq-pkg$(PREFIX)/share/qrq/
	${INSTALL} -d -v                      qrq-pkg$(PREFIX)/share/man/man1/
	${INSTALL} -d -v                      qrq-pkg$(PREFIX)/bin/
	${INSTALL} -d -v                      qrq-pkg/DEBIAN/
	${INSTALL} -s -m 0755 qrq             qrq-pkg$(PREFIX)/bin/
	${INSTALL}    -m 0755 qrqscore        qrq-pkg$(PREFIX)/bin/
	${INSTALL}    -m 0644 qrq.1           qrq-pkg$(PREFIX)/share/man/man1/
	${INSTALL}    -m 0644 callbase.qcb    qrq-pkg$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 english.qcb     qrq-pkg$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 cwops.qcb       qrq-pkg$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 qrqrc           qrq-pkg$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 toplist         qrq-pkg$(PREFIX)/share/qrq/
	${INSTALL}    -m 0644 control         qrq-pkg/DEBIAN/
	export COPYFILE_DISABLE=1; export COPY_EXTENDED_ATTRIBUTES_DISABLE=1; dpkg-deb -b qrq-pkg cydiastore_com.kb1ooo.qrq_v$(shell grep ^Version: control | cut -d ' ' -f 2).deb
	$(SCP) cydiastore_com.kb1ooo.qrq_v$(shell grep ^Version: control | cut -d ' ' -f 2).deb $(IPHONE_HOST):/tmp
	$(SSH) $(IPHONE_HOST) "dpkg -i /tmp/cydiastore_com.kb1ooo.qrq_v$(shell grep ^Version: control | cut -d ' ' -f 2).deb"

clean:
	rm -f qrq.exe qrq test_score test_callbase test_practice test_history test_config test_callbase.qcb test_history.csv toplist-old *~ *.o
	rm -rf Summary
	rm -rf qrq.app

test_score: tests/test_score.c score.c score.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_CFLAGS) -I. -o $@ tests/test_score.c score.c

test_callbase: tests/test_callbase.c callbase.c callbase.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_CFLAGS) -I. -o $@ tests/test_callbase.c callbase.c

test_practice: tests/test_practice.c practice.c practice.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_CFLAGS) -I. -o $@ tests/test_practice.c practice.c

test_history: tests/test_history.c history.c history.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_CFLAGS) -I. -o $@ tests/test_history.c history.c

test_config: tests/test_config.c config.c config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_CFLAGS) -I. -o $@ tests/test_config.c config.c

check: test_score test_callbase test_practice test_history test_config
	./test_score
	./test_callbase
	./test_practice
	./test_history
	./test_config

test: check

print-version:
	@printf '%s\n' '$(VERSION)'

dist:
	sed 's/Version [0-9].[0-9].[0-9]/Version $(VERSION)/g' ../README > README
	mkdir -p ../releases
	rm -f ../releases/qrq-$(VERSION).tar.gz
	rm -rf ../releases/qrq-$(VERSION)
	mkdir qrq-$(VERSION)
	cp qrq.png qrqscore qrq.c score.c score.h callbase.c callbase.h practice.c practice.h history.c history.h config.c config.h qrqrc callbase.qcb toplist \
		../AUTHORS ../ChangeLog README ../COPYING qrq.1 Makefile qrqinstaller.nsi \
		english.qcb cwops.qcb qrq.ico qrq.rc \
		qrq-$(VERSION)
	cp coreaudio.c coreaudio.h oss.c oss.h \
		qrq-$(VERSION)
	cp pulseaudio.h pulseaudio.c qrq-$(VERSION)
	cp -r tests qrq-$(VERSION)
	cp -r OSXExtras qrq-$(VERSION)
	rm -rf qrq-$(VERSION)/OSXExtras/.svn/
	tar -zcf qrq-$(VERSION).tar.gz qrq-$(VERSION)
	mv qrq-$(VERSION) ../releases/
	mv qrq-$(VERSION).tar.gz ../releases/
	md5sum ../releases/*.gz > ../releases/md5sums.txt
