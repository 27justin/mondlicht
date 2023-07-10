#ifndef LISTENERS_H_
#define LISTENERS_H_
#include <stdlib.h>
#include "mondlicht.h"
#include "client.h"
#ifdef XWAYLAND
#include "xwayland.h"
#endif


/* ------------- SURFACE EVENTS -------------- */

void newSurface(struct wl_listener *listener, void *data);
void surfaceMap(struct wl_listener *listener, void *data);
void surfaceUnmap(struct wl_listener *listener, void *data);
void surfaceDestroy(struct wl_listener *listener, void *data);

void surfaceRequestMove(struct wl_listener *listener, void *data);
void surfaceRequestResize(struct wl_listener *listener, void *data);
void surfaceRequestMaximize(struct wl_listener *listener, void *data);
void surfaceRequestFullscreen(struct wl_listener *listener, void *data);

/* ------------- CURSOR EVENTS -------------- */

void cursorMotion(struct wl_listener *listener, void *data);
void cursorAbsoluteMotion(struct wl_listener *listener, void *data);
void cursorButton(struct wl_listener *listener, void *data);
void cursorAxis(struct wl_listener *listener, void *data);
void cursorFrame(struct wl_listener *listener, void *data);

/* ------------- OUTPUT EVENTS -------------- */
void outputFrame(struct wl_listener *listener, void *data);
void outputRequestState(struct wl_listener *listener, void *data);
void outputDestroy(struct wl_listener *listener, void *data);
void newOutput(struct wl_listener *listener, void *data);


/* ------------- INPUT EVENTS -------------- */
void newInput(struct wl_listener *listener, void *data);

void keyboardModifier(struct wl_listener *listener, void *data);
void keyboardKey(struct wl_listener *listener, void *data);
void keyboardDestroy(struct wl_listener *listener, void *data);

void requestCursor(struct wl_listener *listener, void *data);
void requestSetSelection(struct wl_listener *listener, void *data);


#ifdef XWAYLAND
void xwaylandReady(struct wl_listener *listener, void *data);
void newXWaylandSurface(struct wl_listener *listener, void *data);
void destroyXwaylandSurface(struct wl_listener *listener, void *data);
void xwaylandConfigure(struct wl_listener *listener, void *data);
void xwaylandActivate(struct wl_listener *listener, void *data);
void xwaylandSetHints(struct wl_listener *listener, void *data);
void xwaylandAssociate(struct wl_listener *listener, void *data);
void xwaylandDissociate(struct wl_listener *listener, void *data);

#endif

#endif
