#ifndef MOND_CLIENT_H
#define MOND_CLIENT_H
#ifdef XWAYLAND
#include "xwayland.h"
#endif

typedef enum ClientType {
	CLIENT_XDG,
#ifdef XWAYLAND
	CLIENT_XWAYLAND
#endif
} ClientType;

struct Client;
typedef struct Mondlicht Mondlicht;
typedef struct ClientEventManager {
	struct Client *client;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener requestMove;
	struct wl_listener requestResize;
	struct wl_listener requestMaximize;
	struct wl_listener requestFullscreen;

// TODO: Ideally, these should be separated out and only be configured when
// the client is an Xwayland client.
#ifdef XWAYLAND
	struct wl_listener configure;
	struct wl_listener activate;
	struct wl_listener setHints;
	struct wl_listener associate;
	struct wl_listener dissociate;
#endif
} ClientEventManager;

typedef struct Workspace Workspace;
typedef struct Client {
	enum ClientType type;
	struct Mondlicht *root;
	Workspace *workspace;

	union {
		struct wlr_xdg_toplevel *toplevel; // XDG Shell
#ifdef XWAYLAND
		struct wlr_xwayland_surface *xwayland_surface;
#endif
	};

	// Event manager
	ClientEventManager *events;

	struct wlr_scene_tree *sceneTree;

	struct wl_list link;
} Client;

struct wlr_surface *clientSurface(Client*);

void moveClientToWorkspace(Client*, Workspace*);


#endif
