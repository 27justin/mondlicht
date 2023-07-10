#include "layers.h"
#include "utils.h"
#include <stdlib.h>

void newLayerSurface(struct wl_listener *listener, void *data) {
	Mondlicht *root = wl_container_of(listener, root, newLayerSurface);
	struct wlr_layer_surface_v1 *request = data;
	struct wlr_layer_surface_v1_state state = request->pending;

	fprintf(stderr, "newLayerSurface called.\n");

	// The user has requested to map the given surface to a specific layer.
	// requestedSurface->output may be NULL, in which case we default to the output 
	// that the cursor is currently on
	MondlichtOutput *monitor;
	if(request->output == NULL) {
		monitor = hoveredOutput(root);
	}else{
		// Look up the wlr_output inside the monitors
		wl_list_for_each(monitor, &root->outputs, link) {
			if(monitor->output == request->output) {
				break;
			}
		}
	}
	fprintf(stderr, "Monitor for will-be layer surface: %p.\n", monitor);

	if(monitor == NULL) {
		fprintf(stderr, "Couldn't find output for layer surface\n");
		return;
	}

	enum LAYER_SHELL requestedLayer = (enum LAYER_SHELL) state.layer;


	Layer *depositLayer = &root->layers[requestedLayer];
	SurfaceLayer *layer = malloc(sizeof(SurfaceLayer));
	memset(layer, 0, sizeof(SurfaceLayer));
	layer->out = monitor;
	//layer->surface = wlr_scene_surface_create(depositLayer->scene, request->surface);
	layer->surface = wlr_scene_layer_surface_v1_create(depositLayer->scene, request);
	//layer->surface = wlr_scene_subsurface_tree_create(depositLayer->scene, request->surface);
	layer->parent = depositLayer;

	wl_list_insert(&depositLayer->surfaces, &layer->link);
	layer->__protocolSurface = request;

	// Now we attach the surface specific events
	layer->destroy.notify = destroyLayerSurface;
	wl_signal_add(&request->events.destroy, &layer->destroy);

	layer->newPopup.notify = newLayerPopup;
	wl_signal_add(&request->events.new_popup, &layer->newPopup);

	// Tell the surface to configure itself (height, width, etc)
	wlr_layer_surface_v1_configure(request, monitor->output->width, monitor->output->height);
}

void destroyLayerSurface(struct wl_listener *listener, void *data) {
	fprintf(stderr, "destroyLayerSurface called, NO-OP for now.\n");
	SurfaceLayer *layer = wl_container_of(listener, layer, destroy);
	wl_list_remove(&layer->link);
	wl_list_remove(&layer->destroy.link);
	wl_list_remove(&layer->newPopup.link);
	free(layer);
}

void newLayerPopup(struct wl_listener *listener, void *data) {
	fprintf(stderr, "newLayerPopup called, NO-OP for now.\n");
}
