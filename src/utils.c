#include "utils.h"
#include <wlr/backend/session.h>

// Switches the virtual console to given tty
void switchTTY(Mondlicht *root, int tty) {
	wlr_session_change_vt(root->session, tty);
}

MondlichtOutput *hoveredOutput(Mondlicht *root) {
	double x, y;
	x = root->cursor->x;
	y = root->cursor->y;
	struct wlr_output *output = wlr_output_layout_output_at(root->output_layout, x, y);
	fprintf(stderr, "Hovered output: %s\n", output->name);
	fprintf(stderr, "Cursor: %f, %f\n", x, y);
	fprintf(stderr, "Output: %d, %d\n", output->width, output->height);
	MondlichtOutput *local;
	wl_list_for_each(local, &root->outputs, link) {
		if (local->output == output) {
			return local;
		}
	}
	return NULL;
}

