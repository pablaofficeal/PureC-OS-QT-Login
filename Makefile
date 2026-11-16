ROOT_DIR ?= $(abspath ../..)
BIN_DIR ?= $(ROOT_DIR)/bin
BUILD := $(BIN_DIR)/qt-login-build
OUT := $(BIN_DIR)/repo
VERSION := $(shell cat VERSION)
PKGREL ?= 1
JOBS ?= 4
PKG_FILE := $(OUT)/qt-login-$(VERSION)-$(PKGREL)-x86_64.pkg.tar.gz
SIGN_KEY ?= $(HOME)/.config/purec-pkg/signing.pem

.PHONY: all configure package clean

all: $(BUILD)/build.ninja
	nice -n 19 cmake --build $(BUILD) --parallel $(JOBS)

$(BUILD)/build.ninja: CMakeLists.txt
	BIN_DIR=$(BIN_DIR) cmake -G Ninja -S $(CURDIR) -B $(BUILD) \
		-DCMAKE_TOOLCHAIN_FILE=$(ROOT_DIR)/mk/sysroot/toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DQT_HOST_PATH=/usr -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/cmake

package: all
	@mkdir -p $(OUT)
	rm -f $(OUT)/qt-login-*.pkg.tar.gz*
	python3 $(ROOT_DIR)/mk/pkg/mkpkg.py --name qt-login --version $(VERSION)-$(PKGREL) \
		--desc "Graphical login screen (display manager)" --depend "video-backend>=0.2.0" -o $(OUT) \
		--file usr/bin/qt-login=$(BUILD)/qt-login \
		--file usr/lib/init/system/display-manager.service=$(CURDIR)/units/display-manager.service \
		$(foreach f,DejaVuSans.ttf DejaVuSans-Bold.ttf DejaVuSansMono.ttf DejaVuSansMono-Bold.ttf,--file usr/share/qt-login/fonts/$(f)=$(ROOT_DIR)/assets/fonts/$(f))
	if [ -f "$(SIGN_KEY)" ]; then python3 $(ROOT_DIR)/mk/pkg/pkgsign.py sign $(SIGN_KEY) $(PKG_FILE) $(PKG_FILE).sig; fi
	@ls -l $(OUT)/qt-login-*

clean:
	rm -rf $(BUILD)
