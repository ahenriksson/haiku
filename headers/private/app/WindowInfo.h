/*
 * Copyright 2005, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */
#ifndef _WINDOW_INFO_H_
#define _WINDOW_INFO_H_


#include <SupportDefs.h>


struct window_info {
	team_id		team;
    int32   	server_token;

	int32		thread;
	int32		client_token;
	int32		client_port;
	uint32		workspaces;

	int32		layer;			// whatever this is...
    uint32	  	type;			// see below
    uint32      flags;
	int32		window_left;
	int32		window_top;
	int32		window_right;
	int32		window_bottom;

	int32		show_hide_level;
	bool		is_mini;
} _PACKED;

struct client_window_info : window_info {
	char		name[1];
} _PACKED;

enum {
	// taken from Deskbar
	kNormalWindow = 0,
	kDesktopWindow = 1024,
	kMenuWindow = 1025,
	kWindowScreen = 1026,
};

enum window_action {
	B_MINIMIZE_WINDOW,
	B_BRING_TO_FRONT
};

#endif	// _WINDOW_INFO_H_
