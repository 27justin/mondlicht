#ifndef MOND_XWAYLAND_H_
#define MOND_XWAYLAND_H_

#include <wlr/xwayland.h>
#include <X11/Xlib.h>
#include <xcb/xcb_icccm.h>
enum {
	__NET_WM_WINDOW_TYPE_DESKTOP,
	__NET_WM_WINDOW_TYPE_DOCK,
	__NET_WM_WINDOW_TYPE_TOOLBAR,
	__NET_WM_WINDOW_TYPE_MENU,
	__NET_WM_WINDOW_TYPE_UTILITY,
	__NET_WM_WINDOW_TYPE_SPLASH,
	__NET_WM_WINDOW_TYPE_DIALOG,
	__NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	__NET_WM_WINDOW_TYPE_POPUP_MENU,
	__NET_WM_WINDOW_TYPE_TOOLTIP,
	__NET_WM_WINDOW_TYPE_NOTIFICATION,
	__NET_WM_WINDOW_TYPE_COMBO,
	__NET_WM_WINDOW_TYPE_DND,
	__NET_WM_WINDOW_TYPE_NORMAL,
	__NET_WM_WINDOW_TYPE_LAST
};
#include "mondlicht.h"


typedef struct XwaylandClientEventManager {
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener requestMove;
	struct wl_listener requestResize;
	struct wl_listener requestMaximize;
	struct wl_listener requestFullscreen;

	struct wl_listener configure;
	struct wl_listener activate;
	struct wl_listener setHints;
	struct wl_listener associate;
	struct wl_listener dissociate;
} XwaylandClientEventManager;




#endif
