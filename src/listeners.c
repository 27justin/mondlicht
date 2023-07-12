#include "listeners.h"
#include "client.h"
#include "utils.h"
#include "decorations.h"


void newSurface(struct wl_listener *listener, void *data) {
	// Initialize this surface
	Mondlicht *root = wl_container_of(listener, root, newSurface);

	struct wlr_xdg_surface *surface = data;

	if(surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(surface->popup->parent);
		if(parent) {
			struct wlr_scene_tree *parentTree = parent->data;
			surface->data = wlr_scene_xdg_surface_create(parentTree, surface);
			return;
		}
	}

	Client *view = malloc(sizeof(Client));
	memset(view, 0, sizeof(Client));
	view->type = CLIENT_XDG;

	ClientEventManager *cev = view->events = malloc(sizeof(ClientEventManager));
	memset(cev, 0, sizeof(ClientEventManager));
	cev->client = view;


	view->root = root;
	view->toplevel = surface->toplevel;


	// Create the scene tree for this surface.
	// 
	struct wlr_scene_tree *tree;
	MondlichtOutput *out = hoveredOutput(root);
	if(out) {
		Workspace *workspace = out->activeWorkspace;
		tree = workspace->scene;
		view->workspace = workspace;
	} else {
		tree = &root->scene->tree;
	}

	view->sceneTree = wlr_scene_xdg_surface_create(tree, view->toplevel->base);

	view->sceneTree->node.data = view;
	surface->data = view->sceneTree;

	cev->map.notify = surfaceMap;
	wl_signal_add(&surface->surface->events.map, &cev->map);
	
	cev->unmap.notify = surfaceUnmap;
	wl_signal_add(&surface->surface->events.unmap, &cev->unmap);

	cev->destroy.notify = surfaceDestroy;
	wl_signal_add(&surface->events.destroy, &cev->destroy);

	struct wlr_xdg_toplevel *toplevel = surface->toplevel;
	cev->requestMove.notify = surfaceRequestMove;
	wl_signal_add(&toplevel->events.request_move, &cev->requestMove);

	cev->requestResize.notify = surfaceRequestResize;
	wl_signal_add(&toplevel->events.request_resize, &cev->requestResize);

	cev->requestMaximize.notify = surfaceRequestMaximize;
	wl_signal_add(&toplevel->events.request_maximize, &cev->requestMaximize);

	cev->requestFullscreen.notify = surfaceRequestFullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &cev->requestFullscreen);

	return;
}

void surfaceMap(struct wl_listener *listener, void *data) {
	// This surface is ready to be displayed.
	ClientEventManager *cev = wl_container_of(listener, cev, map);
	Client *view = cev->client;

#ifdef XWAYLAND
	if(view->type == CLIENT_XWAYLAND) {
		MondlichtOutput *output = hoveredOutput(view->root);
		if(output)
			view->sceneTree = wlr_scene_tree_create(&view->root->scene->tree);
		else
			view->sceneTree = wlr_scene_tree_create(output->activeWorkspace->scene);
		view->sceneTree->node.data = view;
		wlr_scene_subsurface_tree_create(view->sceneTree, view->xwayland_surface->surface);
	}
#endif

	wl_list_insert(&view->workspace->views, &view->link);

	// Set the position of this wl_surface to be on the workspace's output
	// that the cursor is currently on.
	MondlichtOutput *output = hoveredOutput(view->root);
	int ow, oh;
	wlr_output_effective_resolution(output->output, &ow, &oh);
	double lx, ly;
	wlr_output_layout_output_coords(view->root->output_layout, output->output, &lx, &ly);
	lx = -lx; // TODO: Bug?
			  // Full-HD next to 4k returns -1920 in my case...
	// Set the position to current monitor center
	double X = lx + (ow - clientSurface(view)->current.width) / 2;
	double Y = ly + (oh - clientSurface(view)->current.height) / 2;
	wlr_scene_node_set_position(&view->sceneTree->node, X, Y);

	// Now set up the server-side decoration
	// TODO: Delegate this to a proper decoration manager
	if(view->decoration == NULL && view->type == CLIENT_XDG) {
		struct wlr_box geom;
		wlr_xdg_surface_get_geometry(view->toplevel->base, &geom);
		fprintf(stderr, "XDG-Client geometry:\nwidth: %d\nheight: %d\n", geom.width, geom.height);
		CairoBuffer *buf = newCairoBuffer(geom.width + 20, geom.height + 20);
		fprintf(stderr, "Created cairo buffer\n");
		fprintf(stderr, "Cairo buffer loc: %p\n", buf->data);


		cairo_surface_t *surf = buf->surface;
		cairo_set_source_rgba(buf->cairo, 1.0, 0.0, 0.0, 1.0);
		cairo_rectangle(buf->cairo, 0, 0, geom.width, geom.height);
		cairo_fill(buf->cairo);

		struct wlr_scene_buffer *scene_buf = wlr_scene_buffer_create(view->sceneTree, &buf->base);
		if(!scene_buf) {
			fprintf(stderr, "Failed to create scene buffer\n");
			return;
		}
		wlr_scene_node_raise_to_top(&scene_buf->node);
		wlr_scene_buffer_set_dest_size(scene_buf, geom.width + 20, geom.height + 20);
		wlr_scene_node_set_position(&scene_buf->node, -10, -10);
		wlr_scene_node_set_enabled(&scene_buf->node, true);

		wlr_buffer_drop(&buf->base);
		cairo_destroy(buf->cairo);
	}
	// 	
	// Give the surface focus
	focusSurface(view, clientSurface(view));

}

void surfaceUnmap(struct wl_listener *listener, void *data) {
	ClientEventManager *cev = wl_container_of(listener, cev, unmap);
	Client *view = cev->client;

	if(view->root->grab_agent && view->root->grab_agent->view == view) {
		view->root->cursorMode = CURSOR_PASSTHROUGH;
		free(view->root->grab_agent);
	}

	wl_list_remove(&view->link);
}

void surfaceDestroy(struct wl_listener *listener, void *data) {
	// This surface is ready to be displayed.
	ClientEventManager *cev = wl_container_of(listener, cev, destroy);
	Client *view = cev->client;

	if(!view) {
		fprintf(stderr, "Something went terribly wrong, VIEW is NULL!\n");
		return;
	}

	// map and unmap are not initialized with CLIENT_XWAYLAND
	// as their events are not wl_signal_add'ed, but just assigned instead
	// so we need to check if they are initialized before removing them
	// from the signal list.
	wl_list_remove(&cev->map.link);
	wl_list_remove(&cev->unmap.link);
	wl_list_remove(&cev->destroy.link);
	wl_list_remove(&cev->requestMove.link);
	wl_list_remove(&cev->requestResize.link);
	wl_list_remove(&cev->requestMaximize.link);
	wl_list_remove(&cev->requestFullscreen.link);
	/*if(view->sceneTree)
		wlr_scene_node_destroy(&view->sceneTree->node);*/
	free(cev);
	free(view);
}

void surfaceRequestMove(struct wl_listener *listener, void *data) {
	ClientEventManager *cev = wl_container_of(listener, cev, requestMove);
	Client *view = cev->client;
	struct Mondlicht *root = view->root;

	if(root->grab_agent) {
		free(root->grab_agent);
		root->grab_agent = NULL;
	}

	struct wlr_surface *focus = view->root->seat->pointer_state.focused_surface;
	// Verify that this surface is the focused surface.
	if(focus != wlr_surface_get_root_surface(focus)) {
		return;
	}

	root->grab_agent = malloc(sizeof(struct MondlichtGrabAgent));
	memset(root->grab_agent, 0, sizeof(struct MondlichtGrabAgent));
	root->grab_agent->view = view;
	root->grab_agent->type = GRAB_MOVE;

	root->grab_agent->x = root->cursor->x - view->sceneTree->node.x;
	root->grab_agent->y = root->cursor->y - view->sceneTree->node.y;

	view->root->cursorMode = CURSOR_MOVE;
}

void surfaceRequestResize(struct wl_listener *listener, void *data) {
	ClientEventManager *cev = wl_container_of(listener, cev, requestResize);
	Client *view = cev->client;
	struct Mondlicht *root = view->root;
	struct wlr_xdg_toplevel_resize_event *event = data;

	if(root->grab_agent) {
		free(root->grab_agent);
		root->grab_agent = NULL;
	}

	struct wlr_surface *focus = view->root->seat->pointer_state.focused_surface;
	// Verify that this surface is the focused surface.
	if(focus != wlr_surface_get_root_surface(focus)) {
		return;
	}

	root->grab_agent = malloc(sizeof(struct MondlichtGrabAgent));
	memset(root->grab_agent, 0, sizeof(struct MondlichtGrabAgent));
	root->grab_agent->view = view;
	root->grab_agent->type = GRAB_RESIZE;

	struct wlr_box box;
	wlr_xdg_surface_get_geometry(view->toplevel->base, &box);
	double border_x = (view->sceneTree->node.x + box.x) + ((event->edges & WLR_EDGE_RIGHT) ? box.width : 0);
	double border_y = (view->sceneTree->node.y + box.y) + ((event->edges & WLR_EDGE_BOTTOM) ? box.height : 0);
	root->grab_agent->x = root->cursor->x - border_x;
	root->grab_agent->y = root->cursor->y - border_y;
	box.x += view->sceneTree->node.x;
	box.y += view->sceneTree->node.y;

	root->grab_agent->geobox = box;
	root->grab_agent->resizeEdges = event->edges;
	view->root->cursorMode = CURSOR_RESIZE;
}

void surfaceRequestMaximize(struct wl_listener *listener, void *data) {
	ClientEventManager *cev = wl_container_of(listener, cev, requestMaximize);
	Client *view = cev->client;

	// Retrieve the client outputs dimensions
	struct wlr_box dimensions;
	wlr_output_layout_get_box(view->root->output_layout, view->workspace->monitor->output, &dimensions);
	// Set the client to the dimensions of the output
	wlr_xdg_toplevel_set_size(view->toplevel, dimensions.width, dimensions.height);
	wlr_xdg_toplevel_set_maximized(view->toplevel, true);
	wlr_xdg_surface_schedule_configure(view->toplevel->base);
}

void surfaceRequestFullscreen(struct wl_listener *listener, void *data) {
	ClientEventManager *cev = wl_container_of(listener, cev, requestFullscreen);
	Client *view = cev->client;
	// Ignore event
	struct wlr_box dimensions;
	wlr_output_layout_get_box(view->root->output_layout, view->workspace->monitor->output, &dimensions);

	wlr_xdg_toplevel_set_size(view->toplevel, dimensions.width, dimensions.height);
	wlr_scene_node_set_position(&view->sceneTree->node, dimensions.x, dimensions.y);

	wlr_xdg_toplevel_set_fullscreen(view->toplevel, true);
	wlr_xdg_surface_schedule_configure(view->toplevel->base);
}
static void doCursorResize(Mondlicht *root, uint32_t time) {
	Client *view = root->grab_agent->view;
	double border_x = root->cursor->x - root->grab_agent->x;
	double border_y = root->cursor->y - root->grab_agent->y;
	int newLeft = root->grab_agent->geobox.x;
	int newRight = root->grab_agent->geobox.x + root->grab_agent->geobox.width;
	int newTop = root->grab_agent->geobox.y;
	int newBottom = root->grab_agent->geobox.y + root->grab_agent->geobox.height;

	if (root->grab_agent->resizeEdges & WLR_EDGE_TOP) {
		newTop = border_y;
		if (newTop >= newBottom) {
			newTop = newBottom - 1;
		}
	} else if (root->grab_agent->resizeEdges & WLR_EDGE_BOTTOM) {
		newBottom = border_y;
		if (newBottom <= newTop) {
			newBottom = newTop + 1;
		}
	}
	if (root->grab_agent->resizeEdges & WLR_EDGE_LEFT) {
		newLeft = border_x;
		if (newLeft >= newRight) {
			newLeft = newRight - 1;
		}
	} else if (root->grab_agent->resizeEdges & WLR_EDGE_RIGHT) {
		newRight = border_x;
		if (newRight <= newLeft) {
			newRight = newLeft + 1;
		}
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(view->toplevel->base, &geo_box);
	wlr_scene_node_set_position(&view->sceneTree->node,
		newLeft - geo_box.x, newTop - geo_box.y);

	int newWidth = newRight - newLeft;
	int newHeight = newBottom - newTop;
	wlr_xdg_toplevel_set_size(view->toplevel, newWidth, newHeight);
}

static void doCursorMove(Mondlicht *root, uint32_t time) {
	/* Move the grabbed view to the new position. */
	Client *view = root->grab_agent->view;
	wlr_scene_node_set_position(&view->sceneTree->node,
		root->cursor->x - root->grab_agent->x,
		root->cursor->y - root->grab_agent->y);

	// Handle workspace transfers, if necessary.
	MondlichtOutput* output = hoveredOutput(root);
	if(output->activeWorkspace != view->workspace) {
		moveClientToWorkspace(view, output->activeWorkspace);
	}
}

void cursorMotion(struct wl_listener *listener, void *data) {
	struct Mondlicht *root = wl_container_of(listener, root, cursorMotion);
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(root->cursor, &event->pointer->base, event->delta_x, event->delta_y);

	if(root->grab_agent != NULL) {
		MondlichtGrabAgent *agent = root->grab_agent;
		switch(agent->type) {
			case GRAB_KEYBOARD_MOVE:
			case GRAB_MOVE: {
				doCursorMove(root, event->time_msec);
				return;
							}
			case GRAB_RESIZE: {
				doCursorResize(root, event->time_msec);
				return;
							}
			case GRAB_NONE: {
								// Should never happen.
								return;
							}
		}
	}else{
		double sx, sy;
		struct wlr_seat *seat = root->seat;
		struct wlr_surface *hover = NULL;
		Client *view = surfaceAt(root, root->cursor->x, root->cursor->y, &hover, &sx, &sy);
		if(!view) {
			wlr_cursor_set_xcursor(root->cursor, root->cursorManager, "default");
		}
		if(hover) {
			wlr_seat_pointer_notify_enter(seat, hover, sx, sy);
			wlr_seat_pointer_notify_motion(seat, event->time_msec, sx, sy);
		}else{
			wlr_seat_pointer_clear_focus(seat);
		}
	}
}

void cursorAbsoluteMotion(struct wl_listener *listener, void *data) {
	struct Mondlicht *root = wl_container_of(listener, root, cursorMotionAbsolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(root->cursor, &event->pointer->base, event->x, event->y);
	if(root->grab_agent) {
		struct MondlichtGrabAgent *agent = root->grab_agent;
		switch(agent->type) {
			case GRAB_KEYBOARD_MOVE:
			case GRAB_MOVE: {
				doCursorMove(root, event->time_msec);
							}
			case GRAB_RESIZE: {
				doCursorResize(root, event->time_msec);
							}
			case GRAB_NONE: {
								// Should never happen.
								return;
							}
		}
	}
}

void cursorAxis(struct wl_listener *listener, void *data) {
	Mondlicht *root =
		wl_container_of(listener, root, cursorAxis);
	struct wlr_pointer_axis_event *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(root->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source);
}


void cursorButton(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	Mondlicht *root =
		wl_container_of(listener, root, cursorButton);
	struct wlr_pointer_button_event *event = data;

	// If the client holds down Windows key (WLR_MODIFIER_LOGO) and clicks, we
	// want to move the window.
	uint32_t modifiers = wlr_keyboard_get_modifiers(root->seat->keyboard_state.keyboard);
	if (root->grab_agent == NULL && event->button == 272 &&
			(modifiers & WLR_MODIFIER_LOGO)) {
		double sx, sy;
		struct wlr_surface *surface = NULL;
		Client *view = surfaceAt(root,
				root->cursor->x, root->cursor->y, &surface, &sx, &sy);
		if(surface) {
			// Start an interactive move
			root->grab_agent = malloc(sizeof(struct MondlichtGrabAgent));
			root->grab_agent->type = GRAB_KEYBOARD_MOVE;
			root->grab_agent->view = view;

			// Now we set the x and y of the grab agent
			root->grab_agent->x = root->cursor->x - view->sceneTree->node.x;
			root->grab_agent->y = root->cursor->y - view->sceneTree->node.y;

			root->cursorMode = CURSOR_MOVE;
			return;
		}
	}

	/* Notify the client with pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(root->seat,
			event->time_msec, event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface = NULL;
	Client *view = surfaceAt(root,
			root->cursor->x, root->cursor->y, &surface, &sx, &sy);
	if (event->state == WLR_BUTTON_RELEASED) {
		/* If you released any buttons, we exit interactive move/resize mode. */
		if (root->grab_agent) {
			free(root->grab_agent);
			root->grab_agent = NULL;
			root->cursorMode = CURSOR_PASSTHROUGH;
		}
	} else {
		/* Focus that client if the button was _pressed_ */
		focusSurface(view, surface);
	}
}

void cursorFrame(struct wl_listener *listener, void *data) {
	Mondlicht *root =
		wl_container_of(listener, root, cursorFrame);
	/* Notify the client with pointer focus of a frame event. */
	wlr_seat_pointer_notify_frame(root->seat);
}

void outputFrame(struct wl_listener *listener, void *data) {
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	struct MondlichtOutput *output = wl_container_of(listener, output, frame);
	
	struct wlr_scene_output *scene_output;

	scene_output = wlr_scene_get_scene_output(output->root->scene, output->output);
	if(!scene_output)
		return;

	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

void outputRequestState(struct wl_listener *listener, void *data) {
	struct MondlichtOutput *output = wl_container_of(listener, output, requestState);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->output, event->state);
}

void outputDestroy(struct wl_listener *listener, void *data) {
	struct MondlichtOutput *output = wl_container_of(listener, output, destroy);
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->requestState.link);
	wl_list_remove(&output->destroy.link);
	free(output);
}

void newOutput(struct wl_listener *listener, void *data) {
	struct Mondlicht *root =
		wl_container_of(listener, root, newOutput);
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, root->allocator, root->renderer);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	struct MondlichtOutput *output = malloc(sizeof(struct MondlichtOutput));
	memset(output, 0, sizeof(struct MondlichtOutput));
	output->output = wlr_output;
	output->root = root;

	output->activeWorkspace = &output->workspaces[0];
	for(int i = 0; i < WORKSPACE_COUNT; i++) {
		output->workspaces[i].monitor = output;
		struct wlr_scene_tree *scene = output->workspaces[i].scene = wlr_scene_tree_create(root->layers[TOP].scene);
		// Reorder the sceneTree to be on top of layer BOTTOM, but below TOP
		//wlr_scene_node_place_above(&scene->node, &root->layers[BOTTOM].scene->node);
		wl_list_init(&output->workspaces[i].views);
	}

	/* Sets up a listener for the frame event. */
	output->frame.notify = outputFrame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* Sets up a listener for the state request event. */
	output->requestState.notify = outputRequestState;
	wl_signal_add(&wlr_output->events.request_state, &output->requestState);

	/* Sets up a listener for the destroy event. */
	output->destroy.notify = outputDestroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wlr_output_commit(wlr_output);
	wl_list_insert(&root->outputs, &output->link);

	/* Setup the scale for this monitor */
	// TODO: This has to be configurable.
	if(strcmp(wlr_output->name, "DP-4") == 0) {
		wlr_output_set_scale(wlr_output, 1.0);
		wlr_output_layout_add(root->output_layout, wlr_output, 0, 0);
	}else if(strcmp(wlr_output->name, "HDMI-A-2") == 0) {
		wlr_output_set_scale(wlr_output, 0.25);
		wlr_output_layout_add(root->output_layout, wlr_output, 3840, 0);
	}else{
		wlr_output_layout_add_auto(root->output_layout, wlr_output);
	}


	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
}


static void addKeyboard(Mondlicht *root, struct wlr_input_device *kbd) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(kbd);

	struct MondlichtKeyboard *keyboard = malloc(sizeof(struct MondlichtKeyboard));
	memset(keyboard, 0, sizeof(struct MondlichtKeyboard));
	keyboard->root = root;
	keyboard->keyboard = wlr_keyboard;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	// Initialize de_DE keymap
	const struct xkb_rule_names kbdmap = {
		.rules = "evdev",
		.model = "pc105",
		.layout = "de",
		.variant = "nodeadkeys",
		.options = NULL,
	};
	struct xkb_keymap *keymap = xkb_map_new_from_names(context,
		&kbdmap, XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboardModifier;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboardKey;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboardDestroy;
	wl_signal_add(&kbd->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(root->seat, keyboard->keyboard);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&root->keyboards, &keyboard->link);

}
static void addPointer(Mondlicht *root, struct wlr_input_device *cursor) {
	wlr_cursor_attach_input_device(root->cursor, cursor);
}

void newInput(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct Mondlicht *root =
		wl_container_of(listener, root, newInput);
	struct wlr_input_device *device = data;
	switch (device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			addKeyboard(root, device);
			break;
		case WLR_INPUT_DEVICE_POINTER:
			addPointer(root, device);
			break;
		default:
			break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In TinyWL we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&root->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(root->seat, caps);
}


void requestCursor(struct wl_listener *listener, void *data) {
	struct Mondlicht *root =
		wl_container_of(listener, root, requestCursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		root->seat->pointer_state.focused_client;
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(root->cursor, event->surface,
			event->hotspot_x, event->hotspot_y);
	}
}

void requestSetSelection(struct wl_listener *listener, void *data) {
	struct Mondlicht *root =
		wl_container_of(listener, root, requestSetSelection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(root->seat, event->source, event->serial);
}

void keyboardKey(struct wl_listener *listener, void *data) {
	struct MondlichtKeyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct Mondlicht *root = keyboard->root;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = root->seat;



	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->keyboard->xkb_state, keycode, &syms);
	
	// Check if the key was depressed and its the windows key.
	// If so, we want to check whether root->grab_agent is set, and it's type is GRAB_KEYBOARD_MOVE
	// If so, we want to free root->grab_agent
	if(keycode == 133 && root->grab_agent != NULL) {
		if(root->grab_agent != NULL && root->grab_agent->type == GRAB_KEYBOARD_MOVE) {
			free(root->grab_agent);
			root->grab_agent = NULL;
		}
	}
	
	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);

	/* Hardcoded binding for switching VT's (virtual console) */
	for(int i = XKB_KEY_XF86Switch_VT_1; i < XKB_KEY_XF86Switch_VT_12; i++) {
		if(syms[0] == i && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			int vt = i - XKB_KEY_XF86Switch_VT_1 + 1;
			wlr_session_change_vt(root->session, vt);
			return;
		}
	}


	if ((modifiers & WLR_MODIFIER_LOGO) &&
			event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* If alt is held down and this button was _pressed_, we attempt to
		 * process it as a compositor keybinding. */
		for (int i = 0; i < nsyms; i++) {
			handled = hotkey(root, syms[i]);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

void keyboardDestroy(struct wl_listener *listener, void *data) {
	struct MondlichtKeyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->link);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	free(keyboard);
}

void keyboardModifier(
		struct wl_listener *listener, void *data) {
	struct MondlichtKeyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */

	wlr_seat_set_keyboard(keyboard->root->seat, keyboard->keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(keyboard->root->seat,
		&keyboard->keyboard->modifiers);
}


#ifdef XWAYLAND

static Atom getatom(xcb_connection_t *xc, const char *name) {
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xc, 0, strlen(name), name);
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(xc, cookie, NULL);
	if (reply == NULL) {
		return XCB_ATOM_NONE;
	}
	Atom atom = reply->atom;
	free(reply);
	return atom;
}

void xwaylandReady(struct wl_listener *listener, void *data) {
	dbg_log("Xwayland is ready, setting up atoms");
	struct Mondlicht *root =
		wl_container_of(listener, root, xwaylandReady);
	struct wlr_xwayland *xwayland = root->xwayland;

	dbg_log("Connecting to seat");
	wlr_xwayland_set_seat(xwayland, root->seat);

	dbg_log("Connecting to XCB");
	fprintf(stderr, "C-DEBUG[Y]: display name: %p\n", xwayland);
	xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
	if (xc == NULL || xcb_connection_has_error(xc)) {
		dbg_log("Failed to open Xwayland X11 display");
		wlr_log(WLR_ERROR, "Failed to open Xwayland X11 display");
		return;
	}
	dbg_log("Connected. Retrieving Atoms.");

	// TODO: Move this into a loop or something.
	root->atoms[__NET_WM_WINDOW_TYPE_DESKTOP] = getatom(xc, "_NET_WM_WINDOW_TYPE_DESKTOP");
	root->atoms[__NET_WM_WINDOW_TYPE_DOCK] = getatom(xc, "_NET_WM_WINDOW_TYPE_DOCK");
	root->atoms[__NET_WM_WINDOW_TYPE_TOOLBAR] = getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLBAR");
	root->atoms[__NET_WM_WINDOW_TYPE_MENU] = getatom(xc, "_NET_WM_WINDOW_TYPE_MENU");
	root->atoms[__NET_WM_WINDOW_TYPE_UTILITY] = getatom(xc, "_NET_WM_WINDOW_TYPE_UTILITY");
	root->atoms[__NET_WM_WINDOW_TYPE_SPLASH] = getatom(xc, "_NET_WM_WINDOW_TYPE_SPLASH");
	root->atoms[__NET_WM_WINDOW_TYPE_DIALOG] = getatom(xc, "_NET_WM_WINDOW_TYPE_DIALOG");
	root->atoms[__NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = getatom(xc, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
	root->atoms[__NET_WM_WINDOW_TYPE_POPUP_MENU] = getatom(xc, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
	root->atoms[__NET_WM_WINDOW_TYPE_TOOLTIP] = getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLTIP");
	root->atoms[__NET_WM_WINDOW_TYPE_NOTIFICATION] = getatom(xc, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
	root->atoms[__NET_WM_WINDOW_TYPE_COMBO] = getatom(xc, "_NET_WM_WINDOW_TYPE_COMBO");
	root->atoms[__NET_WM_WINDOW_TYPE_DND] = getatom(xc, "_NET_WM_WINDOW_TYPE_DND");
	root->atoms[__NET_WM_WINDOW_TYPE_NORMAL] = getatom(xc, "_NET_WM_WINDOW_TYPE_NORMAL");

	// Set Xwayland cursor
	dbg_log("Setting up Xcursor");
	struct wlr_xcursor *xcursor;
	if((xcursor = wlr_xcursor_manager_get_xcursor(root->cursorManager, "left_ptr", 1))) {
		wlr_xwayland_set_cursor(root->xwayland, 
			xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
			xcursor->images[0]->width, xcursor->images[0]->height,
			xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y
		);
	}
	dbg_log("Xwayland setup complete");
	xcb_disconnect(xc);
}
void newXWaylandSurface(struct wl_listener *listener, void *data) {
	dbg_log("New XWayland surface");
	struct Mondlicht *root =
		wl_container_of(listener, root, newXwaylandSurface);

	struct wlr_xwayland_surface *xsurface = data;

	Client *view = malloc(sizeof(Client));
	memset(view, 0, sizeof(Client));
	view->type = CLIENT_XWAYLAND;

	ClientEventManager *cev = view->events = malloc(sizeof(ClientEventManager));
	memset(cev, 0, sizeof(ClientEventManager));
	cev->client = view;
	MondlichtOutput *output = hoveredOutput(root);
	if(output)
		view->workspace = output->activeWorkspace;

	view->xwayland_surface = xsurface;
	view->root = root;

	cev->map.notify = surfaceMap;
	cev->unmap.notify = surfaceUnmap;
	cev->destroy.notify = surfaceDestroy;

	// Attach to request configure event
	cev->configure.notify = xwaylandConfigure;
	wl_signal_add(&xsurface->events.request_configure, &cev->configure);

	cev->activate.notify = xwaylandActivate;
	wl_signal_add(&xsurface->events.request_activate, &cev->activate);

	cev->associate.notify = xwaylandAssociate;
	wl_signal_add(&xsurface->events.associate, &cev->associate);

	cev->dissociate.notify = xwaylandDissociate;
	wl_signal_add(&xsurface->events.dissociate, &cev->dissociate);

	cev->requestFullscreen.notify = surfaceRequestFullscreen;
	wl_signal_add(&xsurface->events.request_fullscreen, &cev->requestFullscreen);

	cev->requestMove.notify = surfaceRequestMove;
	wl_signal_add(&xsurface->events.request_move, &cev->requestMove);

	cev->requestResize.notify = surfaceRequestResize;
	wl_signal_add(&xsurface->events.request_resize, &cev->requestResize);

	cev->requestMaximize.notify = surfaceRequestMaximize;
	wl_signal_add(&xsurface->events.request_maximize, &cev->requestMaximize);
	

	dbg_log("Events attached");

	xsurface->data = view;
	dbg_log("XWayland surface added");
}

void xwaylandConfigure(struct wl_listener *listener, void *data) {
	fprintf(stderr, "XWAYLAND: Configure event\n");
	ClientEventManager *cev = wl_container_of(listener, cev, configure);
	Client *view = cev->client;
	struct wlr_xwayland_surface_configure_event *event = data;
	wlr_xwayland_surface_configure(view->xwayland_surface, event->x, event->y,
		event->width, event->height);
}

void xwaylandAssociate(struct wl_listener *listener, void *data) {
	fprintf(stderr, "C-DEBUG: Associate event\n");
	ClientEventManager *cev = wl_container_of(listener, cev, associate);
	Client *view = cev->client;

	// This wlr_xwayland_surface was mapped to a wl_surface
	// we can finally attach the map and unmap events
	wl_signal_add(&view->xwayland_surface->surface->events.map, &cev->map);
	wl_signal_add(&view->xwayland_surface->surface->events.unmap, &cev->unmap);

	// Attach destroy event
	wl_signal_add(&view->xwayland_surface->surface->events.destroy, &cev->destroy);
}

void xwaylandDissociate(struct wl_listener *listener, void *data) {
	fprintf(stderr, "C-DEBUG: Dissociate event\n");
	ClientEventManager *cev = wl_container_of(listener, cev, dissociate);

	// Remove map and unmap listeners
	wl_list_remove(&cev->map.link);
	wl_list_remove(&cev->unmap.link);
	wl_list_remove(&cev->destroy.link);
}

void xwaylandActivate(struct wl_listener *listener, void *data) {
	fprintf(stderr, "XWAYLAND: Activate event\n");
	ClientEventManager *cev = wl_container_of(listener, cev, activate);
	Client *view = cev->client;
	wlr_xwayland_surface_activate(view->xwayland_surface, true);
}
void xwaylandSetHints(struct wl_listener *listener, void *data) {
	// NO-OP for now
}

#endif

