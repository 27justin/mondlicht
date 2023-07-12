#include "mondlicht.h"
#include "listeners.h"
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"
#include "decorations.h"

bool hotkey(struct Mondlicht *root, xkb_keysym_t sym) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 *
	 * This function assumes Alt is held down.
	 */
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(root->wl_display);
		break;
	case XKB_KEY_1:
	case XKB_KEY_2:
	case XKB_KEY_3:
	case XKB_KEY_4:
	case XKB_KEY_5:
	case XKB_KEY_6:
	case XKB_KEY_7:
	case XKB_KEY_8:
	case XKB_KEY_9: {
		/* Get the current monitor */
		MondlichtOutput *output = hoveredOutput(root);
		if (output == NULL) {
			break;
		}else{
			// Map the sym to 1-9
			int workspace = sym - XKB_KEY_1;
			// Disable the old wlr_scene_tree
			wlr_scene_node_set_enabled(&output->activeWorkspace->scene->node, false);

			// Enable the new wlr_scene_tree
			output->activeWorkspace = &output->workspaces[workspace];
			wlr_scene_node_set_enabled(&output->activeWorkspace->scene->node, true);

			// Give first application inside workspace focus
			if(!wl_list_empty(&output->activeWorkspace->views)) {
				struct Client *firstClient = wl_container_of(
					output->activeWorkspace->views.next, firstClient, link);
				focusSurface(firstClient, firstClient->toplevel->base->surface);
			}else{
				fprintf(stderr, "Error, Workspace %d has no views mapped.\n", workspace);
			}
		}
		break;
					}
	case XKB_KEY_F1:
		/* Cycle to the next view */
		if (wl_list_length(&root->views) < 2) {
			break;
		}
		struct Client *next_view = wl_container_of(
			root->views.prev, next_view, link);
		//focusSurface(next_view, next_view->toplevel->base->surface);
		break;
	case XKB_KEY_w:
	case XKB_KEY_W:
		/* Close the focused view if it can be closed */
		if (root->seat->keyboard_state.focused_surface) {
			// Kill the wl_surface
			struct wlr_surface *focusedSurface = root->seat->keyboard_state.focused_surface;
			// Find the surface in the workspaces
			MondlichtOutput *output;
			wl_list_for_each(output, &root->outputs, link) {
				Client *client;
				int nClients = wl_list_length(&output->activeWorkspace->views);
				fprintf(stderr, "Found %d clients in workspace\n", nClients);
				fprintf(stderr, "Views is located here: %p \n", &output->activeWorkspace->views);
				wl_list_for_each(client, &output->activeWorkspace->views, link) {
					struct wlr_surface *surface = clientSurface(client);
					if(surface == focusedSurface) {
						switch(client->type) {
							case CLIENT_XDG:
								fprintf(stderr, "Closing XDG surface\n");
								wlr_xdg_toplevel_send_close(client->toplevel);
								break;
#ifdef XWAYLAND
							case CLIENT_XWAYLAND:
								// TODO:
								break;
#endif
						}
					}
				}
			}
			//wlr_xdg_toplevel_send_close(surface->resource);
		}
		break;
	case XKB_KEY_Return:
	case XKB_KEY_F2: {
		// Spawn `terminology` terminal
		pid_t chproc = fork();
		if (chproc == 0) {
			//execl("/usr/bin/terminology", "/usr/bin/terminology", NULL);
			execl("/usr/bin/foot", "/usr/bin/foot", NULL);
		}
		break;
					 }
	case XKB_KEY_d:
	case XKB_KEY_D:
	case XKB_KEY_F3: {
		// Spawn `dmenu` application launcher
		pid_t chproc = fork();
		if (chproc == 0) {
			execl("/usr/local/bin/fenster", "/usr/local/bin/fenster", "-a", (void*) NULL);

		}
		break;
					 }
	default:
		return false;
	}
	return true;
}


int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);
	char *startup_cmd = NULL;

	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	struct Mondlicht root = {0};
	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	root.wl_display = wl_display_create();
	root.session = wlr_session_create(root.wl_display);
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 root is running. */
	root.backend = wlr_backend_autocreate(root.wl_display, NULL);
	if (root.backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		return 1;
	}

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	root.renderer = wlr_renderer_autocreate(root.backend);
	if (root.renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		return 1;
	}

	wlr_renderer_init_wl_display(root.renderer, root.wl_display);

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	root.allocator = wlr_allocator_autocreate(root.backend,
		root.renderer);
	if (root.allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		return 1;
	}

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces, the subcompositor allows to
	 * assign the role of subsurfaces to surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the handling of the request_set_selection event below.*/
	root.compositor = wlr_compositor_create(root.wl_display, 5, root.renderer);
	wlr_subcompositor_create(root.wl_display);
	wlr_data_device_manager_create(root.wl_display);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	root.output_layout = wlr_output_layout_create();

	//root.mev = malloc(sizeof(struct MondlichtEventManager));

	wl_list_init(&root.outputs);
	root.newOutput.notify = newOutput;
	wl_signal_add(&root.backend->events.new_output, &root.newOutput);

	/* Create a scene graph. This is a wlroots abstraction that handles all
	 * rendering and damage tracking. All the compositor author needs to do
	 * is add things that should be rendered to the scene graph at the proper
	 * positions and then call wlr_scene_output_commit() to render a frame if
	 * necessary.
	 */
	root.scene = wlr_scene_create();
	wlr_scene_attach_output_layout(root.scene, root.output_layout);

	// Initialize the layer shell protocol extension
	root.layerShell = wlr_layer_shell_v1_create(root.wl_display, 4);
	for(int i = 0; i < TOTAL_LAYERS; i++) {
		wl_list_init(&root.layers[i].surfaces);
		root.layers[i].scene = wlr_scene_tree_create(&root.scene->tree);
	}
	// Reorder the layers in the scene tree.
	fprintf(stderr, "BACKGROUND below anything else\n");
	wlr_scene_node_lower_to_bottom(&root.layers[BACKGROUND].scene->node);
	fprintf(stderr, "BOTTOM above BACKGROUND\n");
	wlr_scene_node_place_above(&root.layers[BOTTOM].scene->node, &root.layers[BACKGROUND].scene->node);
	fprintf(stderr, "TOP above BOTTOM\n");
	wlr_scene_node_place_above(&root.layers[TOP].scene->node, &root.layers[BOTTOM].scene->node);
	fprintf(stderr, "OVERLAY above TOP\n");
	wlr_scene_node_place_above(&root.layers[OVERLAY].scene->node, &root.layers[TOP].scene->node);
	

	root.newLayerSurface.notify = newLayerSurface;
	wl_signal_add(&root.layerShell->events.new_surface,
			&root.newLayerSurface);

	fprintf(stderr, "Initializing decorations\n");
	initializeDecorations(&root);
	fprintf(stderr, "Finished initing decorations\n");


	/* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to my
	 * article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	root.xdg_shell = wlr_xdg_shell_create(root.wl_display, 3);
	root.newSurface.notify = newSurface;
	wl_signal_add(&root.xdg_shell->events.new_surface,
			&root.newSurface);


#ifdef XWAYLAND
	dbg_log("Initializing Xwayland");
	root.xwayland = wlr_xwayland_create(root.wl_display, root.compositor,
			/*true*/ false);

	root.xwaylandReady.notify = xwaylandReady;
	wl_signal_add(&root.xwayland->events.ready,
			&root.xwaylandReady);
	root.newXwaylandSurface.notify = newXWaylandSurface;
	wl_signal_add(&root.xwayland->events.new_surface, &root.newXwaylandSurface);
	dbg_log("Attached events");

	fprintf(stderr, "Xwayland display name: %s\n", root.xwayland->display_name);
	setenv("DISPLAY", root.xwayland->display_name, 1);
#endif



	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	root.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(root.cursor, root.output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). */
	root.cursorManager = wlr_xcursor_manager_create(NULL, 24);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in my
	 * input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	root.cursorMode = CURSOR_PASSTHROUGH;
	root.cursorMotion.notify = cursorMotion;
	wl_signal_add(&root.cursor->events.motion, &root.cursorMotion);
	root.cursorMotionAbsolute.notify = cursorAbsoluteMotion;
	wl_signal_add(&root.cursor->events.motion_absolute,
			&root.cursorMotionAbsolute);
	root.cursorButton.notify = cursorButton;
	wl_signal_add(&root.cursor->events.button, &root.cursorButton);
	root.cursorAxis.notify = cursorAxis;
	wl_signal_add(&root.cursor->events.axis, &root.cursorAxis);
	root.cursorFrame.notify = cursorFrame;
	wl_signal_add(&root.cursor->events.frame, &root.cursorFrame);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&root.keyboards);
	root.newInput.notify = newInput;
	wl_signal_add(&root.backend->events.new_input, &root.newInput);
	root.seat = wlr_seat_create(root.wl_display, "seat0");
	root.requestCursor.notify = requestCursor;
	wl_signal_add(&root.seat->events.request_set_cursor,
			&root.requestCursor);
	root.requestSetSelection.notify = requestSetSelection;
	wl_signal_add(&root.seat->events.request_set_selection,
			&root.requestSetSelection);

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(root.wl_display);
	if (!socket) {
		wlr_backend_destroy(root.backend);
		return 1;
	}

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(root.backend)) {
		wlr_backend_destroy(root.backend);
		wl_display_destroy(root.wl_display);
		return 1;
	}

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run the
	 * startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
		}
	}
	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
	wl_display_run(root.wl_display);

	/* Once wl_display_run returns, we destroy all clients then shut down the
	 * root. */
	wl_display_destroy_clients(root.wl_display);
	wlr_scene_node_destroy(&root.scene->tree.node);
	wlr_xcursor_manager_destroy(root.cursorManager);
	wlr_output_layout_destroy(root.output_layout);
	wl_display_destroy(root.wl_display);
#ifdef XWAYLAND
	wlr_xwayland_destroy(root.xwayland);
#endif
	return 0;
}

Client* surfaceAt(Mondlicht *root, double x, double y, struct wlr_surface **tsurface, double *sx, double *sy) {
	struct wlr_scene_node *node = wlr_scene_node_at(&root->scene->tree.node, x, y, sx, sy);
	if (node == NULL) {
		return NULL;
	}

	struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *surface = wlr_scene_surface_try_from_buffer(buffer);
	if (surface == NULL) {
		return NULL;
	}


	struct wlr_surface *surf = surface->surface;
	*tsurface = surf;
	struct wlr_scene_tree *tree = node->parent;
	while(tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree->node.data;
}

void focusSurface(Client *view, struct wlr_surface *surface) {
	/* Note: this function only deals with keyboard focus. */
	if (view == NULL) {
		return;
	}
	struct Mondlicht *root = view->root;
	struct wlr_seat *seat = root->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (prev_surface) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_surface *previous =
			wlr_xdg_surface_try_from_wlr_surface(seat->keyboard_state.focused_surface);
		if(previous != NULL && previous->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
			wlr_xdg_toplevel_set_activated(previous->toplevel, false);
		}
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	/* Move the view to the front */
	wlr_scene_node_raise_to_top(&view->sceneTree->node);

	/* Activate the new surface */
#ifndef XWAYLAND
	wlr_xdg_toplevel_set_activated(view->toplevel, true);
#else
	if(view->type == CLIENT_XDG) {
		wlr_xdg_toplevel_set_activated(view->toplevel, true);
	}else{
		wlr_xwayland_surface_activate(view->xwayland_surface, true);
	}
#endif
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	if (keyboard != NULL) {
#ifndef XWAYLAND
		wlr_seat_keyboard_notify_enter(seat, view->toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
#else
		if(view->type == CLIENT_XDG) {
			wlr_seat_keyboard_notify_enter(seat, view->toplevel->base->surface,
				keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
		}else{
			wlr_seat_keyboard_notify_enter(seat, view->xwayland_surface->surface,
				keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
		}
#endif
	}
}


// TODO: Delete
volatile int dbgCounter = 0;
void dbg_log(const char* msg) {
	fprintf(stderr, "C-DEBUG[%d]: %s\n", ++dbgCounter, msg);
}



