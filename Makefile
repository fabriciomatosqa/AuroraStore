TITLE       := Aurora Store - PS4
VERSION     := 01.00
TITLE_ID    := AURR00001
CONTENT_ID  := IV0000-AURR00001_00-AURORASTORE000001

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
CC          := clang
CXX         := clang++
LD          := ld.lld
PKGDIR      := $(TOOLCHAIN)/bin/linux
INTDIR      := build
SRC         := src
OBJS        := $(INTDIR)/main.o

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib -lc -lkernel -lc++ $(TOOLCHAIN)/lib/crt1.o

all: $(CONTENT_ID).pkg

$(INTDIR):
	mkdir -p $(INTDIR)

$(INTDIR)/main.o: $(SRC)/main.cpp | $(INTDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/aurora.elf $(LDFLAGS)
	$(PKGDIR)/create-fself -in=$(INTDIR)/aurora.elf -out=$(INTDIR)/aurora.oelf --eboot=eboot.bin --paid 0x3800000000000011
	test -s eboot.bin

sce_sys/param.sfo: Makefile
	$(PKGDIR)/PkgTool.Core sfo_new $@
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png catalogs/homebrew.json
	$(PKGDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(PKGDIR)/PkgTool.Core pkg_build $< .

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 sce_sys/param.sfo *.pkg

.PHONY: all clean
