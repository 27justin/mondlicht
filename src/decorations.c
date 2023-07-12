#include "decorations.h"

static const struct wlr_buffer_impl bufferImpl = {
	.destroy = shmBufferDestroy,
	.begin_data_ptr_access = startBufferPtrAccess,
	.end_data_ptr_access = endBufferPtrAccess,
};


ShmBuffer* newShmBuffer(unsigned int width, unsigned int height) {
	int size = 4 * width * height;
	ShmBuffer *buf = malloc(sizeof(ShmBuffer) + size);
	memset(buf, 0, sizeof(ShmBuffer) + size);
	if (buf == NULL) {
		return NULL;
	}
	wlr_buffer_init(&buf->base, &bufferImpl, width, height);
	buf->stride = 4 * width;
	return buf;
}

static ShmBuffer *shmBufferFromWlr(struct wlr_buffer *buffer) {
	if(buffer->impl != &bufferImpl) return NULL;
	return (ShmBuffer*) buffer;
}

static void shmBufferDestroy(struct wlr_buffer *wlr_buffer) {
	ShmBuffer *buf = shmBufferFromWlr(wlr_buffer);
	if(buf)
		free(buf);
}

static bool startBufferPtrAccess(struct wlr_buffer *wlr_buffer, uint32_t flags,
						  void **data, uint32_t *format, size_t *stride) {
	ShmBuffer *buf = shmBufferFromWlr(wlr_buffer);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = (void*) buf->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = buf->stride;
	return true;
}

static void endBufferPtrAccess(struct wlr_buffer *wlr_buffer) {
	// NO-OP
}


CairoBuffer* newCairoBuffer(unsigned int width, unsigned int height) {
	CairoBuffer *buffer = malloc(sizeof(CairoBuffer));
	memset(buffer, 0, sizeof(CairoBuffer));

	/* Allocate the buffer with the scaled size */
	wlr_buffer_init(&buffer->base, &bufferImpl, width, height);
	buffer->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

	cairo_surface_set_device_scale(buffer->surface, 1.0, 1.0);

	buffer->cairo = cairo_create(buffer->surface);
	buffer->data = cairo_image_surface_get_data(buffer->surface);
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = cairo_image_surface_get_stride(buffer->surface);

	if (!buffer->data || cairo_surface_status(buffer->surface) != CAIRO_STATUS_SUCCESS) {
		cairo_destroy(buffer->cairo);
		cairo_surface_destroy(buffer->surface);
		fprintf(stderr, "Failed to allocate cairo buffer\n");
		free(buffer);
		buffer = NULL;
	}
	return buffer;
}




void decorationDestroy(struct wl_listener *listener, void* data) {
	struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
	fprintf(stderr, "decorationDestroy called.\n");
}

void decorationRequestMode(struct wl_listener *listener, void* data) {
	XdgDecoration *deco = wl_container_of(listener, deco, requestMode);
	deco->client->decoration = deco;
	enum wlr_xdg_toplevel_decoration_v1_mode client_mode =
		deco->decoration->requested_mode;


	// Now we initialize the window decoration and add it to the scene graph of this window.
	Client *client = deco->client;

#ifdef XWAYLAND
	if(client->type == CLIENT_XWAYLAND) {
		fprintf(stderr, "Decorating xwayland clients is not supported yet.\n");
		wlr_xdg_toplevel_decoration_v1_set_mode(deco->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
		return;
	}
#endif
	
	// We honour whatever the client prefers.
	wlr_xdg_toplevel_decoration_v1_set_mode(deco->decoration, client_mode);

	/*
	struct wlr_box geom;
	wlr_xdg_surface_get_geometry(client->toplevel->base, &geom);
	CairoBuffer *buf = newCairoBuffer(geom.width + 30, geom.height + 30);
	struct wlr_scene_buffer *sceneBuffer = wlr_scene_buffer_create(client->sceneTree, &buf->base);
	struct wlr_scene_tree *decoration = wlr_scene_tree_create(client->sceneTree);
	//wlr_scene_node_lower_to_bottom(&decoration->node);
	wlr_scene_node_raise_to_top(&decoration->node);
	wlr_scene_node_set_position(&decoration->node, geom.x - 15, geom.y - 15);

	// Draw the decoration using cairo
	cairo_t *cairo = buf->cairo;
	cairo_set_source_rgba(cairo, 1.0, 1.0, 0.5, 1.0);
	cairo_rectangle(cairo, 0, 0, geom.width + 30, geom.height + 30);
	cairo_fill(cairo);

	// Flush the 
	wlr_buffer_unlock(&buf->base);
	*/


}



void newDecoration(struct wl_listener *listener, void* data) {
	struct wlr_xdg_toplevel_decoration_v1 *requestDecoration = data;
	Client *client = requestDecoration->surface->data;
	if(client == NULL) {
		wlr_xdg_toplevel_decoration_v1_set_mode(requestDecoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
		fprintf(stderr, "Could not find client for decoration.\n");
		return;
	}

	XdgDecoration *decoration = malloc(sizeof(XdgDecoration));
	memset(decoration, 0, sizeof(XdgDecoration));
	decoration->decoration = requestDecoration;
	decoration->client = client;

	decoration->requestMode.notify = decorationRequestMode;
	decoration->destroy.notify = decorationDestroy;
	wl_signal_add(&requestDecoration->events.destroy, &decoration->destroy);
	wl_signal_add(&requestDecoration->events.request_mode, &decoration->requestMode);
	decorationRequestMode(&decoration->requestMode, decoration);
}


void initializeDecorations(Mondlicht *root) {
	if(!root->wl_display) return;
	root->decorationManager = wlr_xdg_decoration_manager_v1_create(root->wl_display);
	root->newDecoration.notify = newDecoration;
	root->decorationDestroy.notify = decorationDestroy;
	root->decorationConfigure.notify = decorationRequestMode;
	wl_signal_add(&root->decorationManager->events.new_toplevel_decoration, &root->newDecoration);
	fprintf(stderr, "Initialized decorations.\n");
}



