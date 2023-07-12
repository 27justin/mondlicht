#ifndef MOND_DECORATIONS_H_
#define MOND_DECORATIONS_H_
#include "mondlicht.h"
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <drm_fourcc.h>
#include <cairo.h>
#include <wlr/interfaces/wlr_buffer.h>

typedef struct ShmBuffer {
	struct wlr_buffer base;
	size_t stride;

	uint32_t data[];
} ShmBuffer;

typedef struct CairoBuffer {
	struct wlr_buffer base;
	size_t stride;
	cairo_t *cairo;
	cairo_surface_t *surface;
	uint32_t format;

	void *data;
} CairoBuffer;

typedef struct XdgDecoration {
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct Client *client;
	// TODO: Temporary hack, buffer gets initialized on map
	CairoBuffer *buffer;


	struct wl_listener requestMode;
	struct wl_listener destroy;
} XdgDecoration;


void initializeDecorations(Mondlicht*);

void newDecoration(struct wl_listener*, void*);
void destroyDecoration(struct wl_listener*, void*);

static ShmBuffer* newShmBuffer(unsigned int, unsigned int);
static void shmBufferDestroy(struct wlr_buffer*);
static bool startBufferPtrAccess(struct wlr_buffer*, uint32_t, void**, uint32_t*, size_t*);
static void endBufferPtrAccess(struct wlr_buffer*);

CairoBuffer* newCairoBuffer(unsigned int, unsigned int);
static void cairoBufferDestroy(struct wlr_buffer*);


#endif
