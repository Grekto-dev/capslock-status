CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS ?=
LIBS = -lX11 -ldl

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share

TARGET = capslock-status
GUI_TARGET = capslock-status-gui
LEGACY_TARGET = capslock-indicator
LEGACY_GUI_TARGET = capslock-indicator-gui

SRCS = src/main.c src/indicator.c src/config.c src/osd.c src/tray.c src/notify.c src/sound.c src/i18n.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET) $(GUI_TARGET) $(LEGACY_TARGET) $(LEGACY_GUI_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(GUI_TARGET): src/gui.py
	cp src/gui.py $@
	chmod +x $@

$(LEGACY_TARGET): $(TARGET)
	ln -sf $(TARGET) $@

$(LEGACY_GUI_TARGET): $(GUI_TARGET)
	ln -sf $(GUI_TARGET) $@

%.o: %.c src/*.h
	$(CC) $(CFLAGS) -c $< -o $@

strip: all
	strip --strip-all $(TARGET)

debug: CFLAGS = -Wall -Wextra -g -O0 -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
debug: clean $(TARGET)

install: $(TARGET) $(GUI_TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -m 755 $(GUI_TARGET) $(DESTDIR)$(BINDIR)/$(GUI_TARGET)
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/$(LEGACY_TARGET)
	ln -sf $(GUI_TARGET) $(DESTDIR)$(BINDIR)/$(LEGACY_GUI_TARGET)
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 data/capslock-status.desktop $(DESTDIR)$(DATADIR)/applications/capslock-status.desktop
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps
	install -m 644 data/capslock-status.svg $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/capslock-status.svg
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/64x64/apps
	install -m 644 data/logo.png $(DESTDIR)$(DATADIR)/icons/hicolor/64x64/apps/capslock-status.png

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(BINDIR)/$(GUI_TARGET)
	rm -f $(DESTDIR)$(BINDIR)/$(LEGACY_TARGET)
	rm -f $(DESTDIR)$(BINDIR)/$(LEGACY_GUI_TARGET)
	rm -f $(DESTDIR)$(DATADIR)/applications/capslock-status.desktop
	rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/capslock-status.svg
	rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/64x64/apps/capslock-status.png

test: $(TARGET)
	@echo "=== Executando Testes de Validação do CapsLock Status ==="
	@echo "1. Testando exibição de versão..."
	./$(TARGET) --version
	@echo "2. Testando consulta de status..."
	./$(TARGET) --status
	@echo "3. Testando formato JSON..."
	./$(TARGET) --status --json
	@echo "4. Testando criação de config e teste OSD..."
	./$(TARGET) --test
	@echo "=== Todos os testes passaram com sucesso! ==="

DEB_ARCH ?= $(shell dpkg --print-architecture 2>/dev/null || echo amd64)
DEB_VERSION = 1.0.0
DEB_PACKAGE = capslock-status_$(DEB_VERSION)_$(DEB_ARCH).deb

deb: all strip
	@echo "=== Criando pacote Debian (.deb) ==="
	rm -rf build/deb
	install -d build/deb/DEBIAN
	install -d build/deb/usr/bin
	install -d build/deb/usr/share/applications
	install -d build/deb/usr/share/icons/hicolor/scalable/apps
	install -d build/deb/usr/share/icons/hicolor/64x64/apps
	install -m 755 $(TARGET) build/deb/usr/bin/$(TARGET)
	install -m 755 $(GUI_TARGET) build/deb/usr/bin/$(GUI_TARGET)
	ln -sf $(TARGET) build/deb/usr/bin/$(LEGACY_TARGET)
	ln -sf $(GUI_TARGET) build/deb/usr/bin/$(LEGACY_GUI_TARGET)
	install -m 644 data/capslock-status.desktop build/deb/usr/share/applications/capslock-status.desktop
	install -m 644 data/capslock-status.svg build/deb/usr/share/icons/hicolor/scalable/apps/capslock-status.svg
	install -m 644 data/logo.png build/deb/usr/share/icons/hicolor/64x64/apps/capslock-status.png
	@echo "Package: capslock-status" > build/deb/DEBIAN/control
	@echo "Version: $(DEB_VERSION)" >> build/deb/DEBIAN/control
	@echo "Section: utils" >> build/deb/DEBIAN/control
	@echo "Priority: optional" >> build/deb/DEBIAN/control
	@echo "Architecture: $(DEB_ARCH)" >> build/deb/DEBIAN/control
	@echo "Maintainer: Pedro <176182924+Grekto-dev@users.noreply.github.com>" >> build/deb/DEBIAN/control
	@echo "Depends: libc6, libx11-6, libcairo2, python3, python3-gi, gir1.2-gtk-3.0" >> build/deb/DEBIAN/control
	@echo "Description: Ultra-lightweight CapsLock / NumLock / ScrollLock tray indicator and OSD" >> build/deb/DEBIAN/control
	@echo " CapsLock Status is a native, ultra-lightweight utility for Linux to display" >> build/deb/DEBIAN/control
	@echo " the state of the Caps Lock, Num Lock, and Scroll Lock keys with system tray" >> build/deb/DEBIAN/control
	@echo " icons and smooth on-screen display (OSD) notifications." >> build/deb/DEBIAN/control
	@echo " ." >> build/deb/DEBIAN/control
	@echo " Based on the original CapsLockIndicator project by Jonas Kohl." >> build/deb/DEBIAN/control
	dpkg-deb --build --root-owner-group build/deb $(DEB_PACKAGE)
	@echo "=== Pacote criado com sucesso: $(DEB_PACKAGE) ==="

clean:
	rm -rf $(OBJS) $(TARGET) $(GUI_TARGET) $(LEGACY_TARGET) $(LEGACY_GUI_TARGET) build *.deb

.PHONY: all clean debug strip install uninstall test deb
