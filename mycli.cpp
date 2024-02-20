#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

//wayland
#include "wayland-client.h"
#include "xdg-shell-client-protocol.h"
#include "wp-single-pixel-buffer-v1-client-protocol.h"
#include "viewporter-client-protocol.h"

#include <stdexcept>
#include <cstdlib>
#include <cstring>


#define CHECK_WL_RESULT(_expr) \
if (!(_expr)) \
{ \
    printf("Error executing %s.", #_expr); \
}

void wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    /* Sent by the compositor when it's no longer using this buffer */
    wl_buffer_destroy(wl_buffer);
}

struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};


struct wl_buffer *draw_pixel(struct client_state *state)
{
	struct wl_buffer *buffer = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(state->single_pixel_buffer_manager, 90, 34, 139, UINT32_MAX);

	
	//wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}

void vp_expand(struct client_state *state)
{
	struct wp_viewport *viewport = wp_viewporter_get_viewport(state->viewporter, state->wl_surface);
	wp_viewport_set_source(viewport, 0, 0, state->width, state->height);
	wp_viewport_set_destination(viewport, state->width, state->height);

	wl_surface_commit(state->wl_surface);
	wp_viewport_destroy(viewport);
}


void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct client_state *state = (client_state *)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    struct wl_buffer *buffer = draw_pixel(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_commit(state->wl_surface);

	vp_expand(state);
}

struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial){
    xdg_wm_base_pong(xdg_wm_base, serial);
}

struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

void registry_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
	struct client_state *state = (client_state *)data;
    if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = (wl_shm *)wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
    }
	else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor = (wl_compositor *)wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
    }
	else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = (xdg_wm_base *)wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
    }
	else if (strcmp(interface, wp_single_pixel_buffer_manager_v1_interface.name) == 0) {
		state->single_pixel_buffer_manager = (wp_single_pixel_buffer_manager_v1 *)wl_registry_bind(wl_registry, name,
			&wp_single_pixel_buffer_manager_v1_interface, 1);
	}
	else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
		state->viewporter = (wp_viewporter *)wl_registry_bind(wl_registry, name, &wp_viewporter_interface, 1);
	}
}

void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name){
    
}

struct wl_registry_listener wl_registry_listener = {
	.global = registry_global,
    .global_remove = registry_global_remove,
};

client_state state; //allow for use of state.

int main(int argc, char *argv[])
{
	state.width = 1280;
 	state.height = 720;


	state.wl_display = wl_display_connect(NULL); //look for active wayland server
	if(!state.wl_display){
		printf("could not find wayland server");
		return -1;
	}

	state.wl_registry = wl_display_get_registry(state.wl_display);
	wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
	wl_display_roundtrip(state.wl_display);

	CHECK_WL_RESULT(state.wl_surface = wl_compositor_create_surface(state.wl_compositor));
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);
	xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_set_title(state.xdg_toplevel, "wayland client");
    wl_surface_commit(state.wl_surface);

    std::cout << "disp-mycli" << state.wl_display << "\n";
	std::cout << "srfc-mycli" << state.wl_surface << "\n";


	while (wl_display_dispatch(state.wl_display)) {
        /* This space deliberately left blank */
    }


	//janitor
	wl_surface_destroy(state.wl_surface);
	wl_display_disconnect(state.wl_display);
	return 0;
}
//g++ -o mycli mycli.cpp ./include/xdg-shell-protocol.c ./include/wp-single-pixel-buffer-v1-client-protocol.c ./include/viewporter-client-protocol.c -lwayland-client -lrt -lxkbcommon -lpthread -Wextra -Wall -DWLR_USE_UNSTABLE
