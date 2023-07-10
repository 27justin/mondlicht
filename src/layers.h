#ifndef MOND_LAYERS_H_
#define MOND_LAYERS_H_
#include <wlr/types/wlr_layer_shell_v1.h>

// From wlr-layer-shell-unstable-v1-protocol.h
enum LAYER_SHELL {
	BACKGROUND,
	BOTTOM,
	TOP,
	OVERLAY,
	TOTAL_LAYERS
};

typedef struct MondlichtOutput MondlichtOutput;
// This is the object that hold 0..N SurfaceLayers inside it
typedef struct Layer {
	struct wlr_scene_tree *scene;
	struct wl_list surfaces;
} Layer;
typedef struct SurfaceLayer {
	unsigned int type;
	struct wlr_box geom;
	MondlichtOutput *out;
	//struct wlr_scene_tree *surface;
	//struct wlr_scene_surface *surface;
	struct wlr_scene_layer_surface_v1 *surface;
	Layer *parent;
	struct wl_list link;

	struct wlr_layer_surface_v1 *__protocolSurface;


	//struct wl_listener map;
	//struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener newPopup;
	struct wl_listener surfaceCommit;
} SurfaceLayer;

/* ------------------ LAYER SHELL EVENTS ------------------ */

void newLayerSurface(struct wl_listener *listener, void *data);
void destroyLayerSurface(struct wl_listener *listener, void *data);
void newLayerPopup(struct wl_listener *listener, void *data);


#endif
