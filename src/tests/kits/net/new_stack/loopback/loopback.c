/* loopback.c - loopback interface module 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/dirent.h>
#include <sys/stat.h>

#include <Drivers.h>

#include "net_stack.h"
#include "net_layer.h"

status_t 	std_ops(int32 op, ...);

struct net_layer_module_info nlmi;

static struct net_stack_module_info *g_stack = NULL;


// -------------------
status_t init(void * params)
{
	net_layer *layer;

	layer = malloc(sizeof(*layer));
	if (!layer)
		return B_NO_MEMORY;

	layer->name   = "*/loopback";
	layer->module = &nlmi;
			
	return g_stack->register_layer(layer);
}

status_t uninit(net_layer *me)
{
	printf("%s: uniniting layer\n", me->name);
	free(me);
	return B_OK;
}


status_t enable(net_layer *me, bool enable)
{
	return B_OK;
}


status_t output_buffer(net_layer *me, net_buffer *buffer)
{
	if (!buffer)
		return B_ERROR;
		
	return g_stack->push_buffer_up(me, buffer);
}

// #pragma mark -

status_t std_ops(int32 op, ...) 
{
	switch(op) {
		case B_MODULE_INIT:
			printf("loopback: B_MODULE_INIT\n");
			return get_module(NET_STACK_MODULE_NAME, (module_info **) &g_stack);
			
		case B_MODULE_UNINIT:
			printf("loopback: B_MODULE_UNINIT\n");
			put_module(NET_STACK_MODULE_NAME);
			break;
			
		default:
			return B_ERROR;
	}
	return B_OK;
}

struct net_layer_module_info nlmi = {
	{
		NET_LAYER_MODULE_ROOT "interfaces/loopback",
		0,
		std_ops
	},
	
	init,
	uninit,
	enable,
	output_buffer,
	NULL	// interface layer: no input_buffer
};

_EXPORT module_info *modules[] = {
	(module_info*) &nlmi,	// net_layer_module_info
	NULL
};
