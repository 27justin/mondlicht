#include "client.h"
#include "mondlicht.h"
#include "wayland-util.h"

inline struct wlr_surface *clientSurface(Client *client) {
	switch (client->type) {
		case CLIENT_XDG:
			return client->toplevel->base->surface;
#ifdef XWAYLAND
		case CLIENT_XWAYLAND:
			return client->xwayland_surface->surface;
#endif
		}
}

void moveClientToWorkspace(Client *client, Workspace *workspace) {
	if (client->workspace == workspace) {
		return;
	}

	// Remove link to the old workspace
	wl_list_remove(&client->link);
	wl_list_insert(&workspace->views, &client->link);

	// Remove the client's sceneTree node from the old workspace
	wlr_scene_node_reparent(&client->sceneTree->node, workspace->scene);
	client->workspace = workspace;
}
