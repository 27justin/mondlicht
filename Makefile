CC := clang
CFLAGS := -Wall -Wextra -Werror -g -O0 -Wno-unused-parameter
CFLAGS := -Wno-unused-function # <- For wlroots specific protocols (e.g. wlr-layer-shell)
CFLAGS += -DWLR_USE_UNSTABLE -fsanitize=address 
CFLAGS += -DXWAYLAND
CFLAGS += $(shell pkg-config --cflags wlroots) \
		  $(shell pkg-config --cflags cairo)
CFLAGS += -Ibuild/wayland-protocols


#WAYLAND_PROTOCOLS := xdg-shell xdg-output xdg-decoration content-type ext-idle-notify xwayland-shell \
					 wlr-layer-shell-unstable-v1.xml

WAYLAND_PROTOCOLS := xdg-shell xwayland-shell wlr-layer-shell-unstable-v1.xml

WAYLAND_PROTOCOL_HEADERS := $(addsuffix -protocol.h,$(WAYLAND_PROTOCOLS))
WAYLAND_PROTOCOL_OBJECTS := $(WAYLAND_PROTOCOL_HEADERS:.h=.o)
WAYLAND_PROTOCOL_DIR := $(shell pkg-config --variable=pkgdatadir wayland-protocols)


#LDFLAGS := $(shell pkg-config --libs wlroots)

LDFLAGS := $(shell pkg-config --libs wayland-server) \
		   $(shell pkg-config --libs xkbcommon) \
		   $(shell pkg-config --libs cairo) \

# Temporary workaround against old, stable, version of wlroots
LDFLAGS += -l:libwlroots.so.12

# TODO:
# Make this easier to configure
# Should be disabled when compiling without -DXWAYLAND
LDFLAGS += $(shell pkg-config --libs xcb)


SOURCES := mondlicht.c listeners.c utils.c layers.c client.c decorations.c

BUILD_DIR = build

all: build

protocol-dir:
	mkdir -p $(BUILD_DIR)/wayland-protocols

# TODO:
# This causes the protocol files to be recompiled every time, even if they haven't changed
# which is really horrible and slows the build process down.
$(WAYLAND_PROTOCOLS): protocol-dir
	./protocols.sh $@ $(BUILD_DIR)/wayland-protocols
	
all-protocols: $(WAYLAND_PROTOCOLS)

build-dir:
	mkdir -p $(BUILD_DIR)

$(SOURCES:.c=.o): build-dir
	$(CC) $(CFLAGS) -c src/$(@:.o=.c) -o $(BUILD_DIR)/$@

$(WAYLAND_PROTOCOL_OBJECTS): build-dir $(WAYLAND_PROTOCOLS)
	$(CC) $(CFLAGS) -c $(BUILD_DIR)/wayland-protocols/$(shell echo $@ | sed -e 's/.xml//' -e 's/\.o/\.c/') -o $(BUILD_DIR)/wayland-protocols/$@

build: build-dir $(WAYLAND_PROTOCOL_OBJECTS) $(SOURCES:.c=.o)
	$(CC) $(CFLAGS) $(LDFLAGS) $(wildcard $(BUILD_DIR)/*.o) $(wildcard $(BUILD_DIR)/wayland-protocols/*.o) -o $(BUILD_DIR)/mondlicht

clean:
	rm $(BUILD_DIR)/$(SOURCES:.c=.o) $(BUILD_DIR)/mondlicht
	rm $(BUILD_DIR)/wayland-protocols/*.o $(BUILD_DIR)/wayland-protocols/*.c $(BUILD_DIR)/wayland-protocols/*.h

