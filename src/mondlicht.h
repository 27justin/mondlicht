#ifndef MONDLICHT_H_
#define MONDLICHT_H_

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

#include "client.h"
#include "layers.h"

#ifdef XWAYLAND
#include "xwayland.h"
#endif


typedef enum GrabType {
	GRAB_NONE,
	GRAB_MOVE,
	GRAB_RESIZE,
	GRAB_KEYBOARD_MOVE // Move initiated by keyboard
} GrabType;

typedef struct Client Client;
typedef struct MondlichtGrabAgent {
	Client *view;
	enum GrabType type;
	double x, y;
	struct wlr_box geobox;
	uint32_t resizeEdges;
} MondlichtGrabAgent;

typedef enum CursorMode {
	CURSOR_PASSTHROUGH,
	CURSOR_MOVE,
	CURSOR_RESIZE,
} CursorMode;

struct Mondlicht;
typedef struct MondlichtEventManager {
	struct Mondlicht *root;
	struct wl_listener newSurface;

} MondlichtEventManager;

typedef struct Mondlicht {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_session *session;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;

	struct wlr_scene *scene;
	Layer layers[TOTAL_LAYERS];

	struct wlr_layer_shell_v1 *layerShell;
	struct wl_listener newLayerSurface;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_xdg_decoration_manager_v1 *decorationManager;
	struct wl_listener newDecoration;
	struct wl_listener decorationDestroy;
	struct wl_listener decorationConfigure;



#ifdef XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_listener xwaylandReady;
	struct wl_listener newXwaylandSurface;
	Atom atoms[__NET_WM_WINDOW_TYPE_LAST];
#endif

	MondlichtEventManager *mev;
	struct wl_listener newSurface;

	struct wl_list views;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursorManager;
	struct wl_listener cursorMotion;
	struct wl_listener cursorMotionAbsolute;
	struct wl_listener cursorButton;
	struct wl_listener cursorAxis;
	struct wl_listener cursorFrame;

	struct wlr_seat *seat;
	struct wl_listener newInput;
	struct wl_listener requestCursor;
	struct wl_listener requestSetSelection;
	struct wl_list keyboards;
	CursorMode cursorMode;

	MondlichtGrabAgent *grab_agent;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener newOutput;

} Mondlicht;

typedef MondlichtOutput MondlichtOutput;
typedef struct Workspace {
	struct MondlichtOutput *monitor;
	struct wlr_scene_tree *scene;
	struct wl_list views; // wlr_surface
} Workspace;

typedef struct MondlichtOutput {
	struct wl_list link;
	struct Mondlicht *root;
#define WORKSPACE_COUNT 9
	Workspace workspaces[WORKSPACE_COUNT];
	Workspace* activeWorkspace;

	struct wlr_output *output;
	struct wl_listener frame;
	struct wl_listener requestState;
	struct wl_listener destroy;
} MondlichtOutput;

struct MondlichtKeyboard {
	struct wl_list link;
	struct Mondlicht *root;
	struct wlr_keyboard *keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

Client* surfaceAt(Mondlicht *root, double x, double y, struct wlr_surface**, double *sx, double *sy);
bool hotkey(Mondlicht *root, xkb_keysym_t sym);
void focusSurface(Client *view, struct wlr_surface *surface);


void dbg_log(const char* msg);
#endif
