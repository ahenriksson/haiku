/*
 * Copyright 2004-2005 Haiku, Inc.
 * Distributed under the terms of the Haiku License.
 *
 * common.c:
 * PS/2 hid device driver
 * Authors (in chronological order):
 *		Stefano Ceccherini (burton666@libero.it)
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include <string.h>

#include "common.h"
#include "ps2_service.h"
#include "ps2_dev.h"

int32 api_version = B_CUR_DRIVER_API_VERSION;

device_hooks sKeyboardDeviceHooks = {
	keyboard_open,
	keyboard_close,
	keyboard_freecookie,
	keyboard_ioctl,
	keyboard_read,
	keyboard_write,
	NULL,
	NULL,
	NULL,
	NULL
};

device_hooks sMouseDeviceHooks = {
	mouse_open,
	mouse_close,
	mouse_freecookie,
	mouse_ioctl,
	mouse_read,
	mouse_write,
	NULL,
	NULL,
	NULL,
	NULL
};


isa_module_info *gIsa = NULL;
sem_id gDeviceOpenSemaphore;

static sem_id sKbcSem;
static int32 sIgnoreInterrupts = 0;
static bool sMultiplexingActive = false;


inline uint8
ps2_read_ctrl()
{
	return gIsa->read_io_8(PS2_PORT_CTRL);
}


inline uint8
ps2_read_data()
{
	return gIsa->read_io_8(PS2_PORT_DATA);
}


inline void
ps2_write_ctrl(uint8 ctrl)
{
	gIsa->write_io_8(PS2_PORT_CTRL, ctrl);
}


inline void
ps2_write_data(uint8 data)
{
	gIsa->write_io_8(PS2_PORT_DATA, data);
}


status_t
ps2_wait_read()
{
	int i;
	for (i = 0; i < PS2_CTRL_WAIT_TIMEOUT / 50; i++) {
		if (ps2_read_ctrl() & PS2_STATUS_OUTPUT_BUFFER_FULL)
			return B_OK;
		snooze(50);
	}
	return B_ERROR;
}


status_t
ps2_wait_write()
{
	int i;
	for (i = 0; i < PS2_CTRL_WAIT_TIMEOUT / 50; i++) {
		if (!(ps2_read_ctrl() & PS2_STATUS_INPUT_BUFFER_FULL))
			return B_OK;
		snooze(50);
	}
	return B_ERROR;
}


//	#pragma mark -

void
ps2_flush()
{
	int i;

	acquire_sem(sKbcSem);
	atomic_add(&sIgnoreInterrupts, 1);

	for (i = 0; i < 64; i++) {
		uint8 ctrl;
		uint8 data;
		ctrl = ps2_read_ctrl();
		if (!(ctrl & PS2_STATUS_OUTPUT_BUFFER_FULL))
			return;
		data = ps2_read_data();
		TRACE(("ps2_flush: ctrl 0x%02x, data 0x%02x (%s)\n", ctrl, data, (ctrl & PS2_STATUS_MOUSE_DATA) ? "mouse" : "keyb"));
		snooze(100);
	}

	atomic_add(&sIgnoreInterrupts, -1);
	release_sem(sKbcSem);
}


status_t
ps2_command(uint8 cmd, const uint8 *out, int out_count, uint8 *in, int in_count)
{
	status_t res;
	int i;
	
	acquire_sem(sKbcSem);
	atomic_add(&sIgnoreInterrupts, 1);

	res = ps2_wait_write();
	if (res == B_OK)
		ps2_write_ctrl(cmd);
	
	for (i = 0; res == B_OK && i < out_count; i++) {
		res = ps2_wait_write();
		if (res == B_OK)
			ps2_write_data(out[i]);
	}

	for (i = 0; res == B_OK && i < in_count; i++) {
		res = ps2_wait_read();
		if (res == B_OK)
			in[i] = ps2_read_data();
	}

	atomic_add(&sIgnoreInterrupts, -1);
	release_sem(sKbcSem);
	
	return res;
}


status_t
ps2_keyboard_command(uint8 cmd, const uint8 *out, int out_count, uint8 *in, int in_count)
{

	return B_OK;
}


status_t
ps2_mouse_command(uint8 cmd, const uint8 *out, int out_count, uint8 *in, int in_count)
{

	return B_OK;
}


//	#pragma mark -

inline status_t
ps2_get_command_byte(uint8 *byte)
{
	return ps2_command(PS2_CTRL_READ_CMD, NULL, 0, byte, 1);
}


inline status_t
ps2_set_command_byte(uint8 byte)
{
	return ps2_command(PS2_CTRL_WRITE_CMD, &byte, 1, NULL, 0);
}
	

//	#pragma mark -


static int32 
ps2_interrupt(void* cookie)
{
	uint8 ctrl;
	uint8 data;
	ps2_dev *dev;
	
	ctrl = ps2_read_ctrl();
	if (!(ctrl & PS2_STATUS_OUTPUT_BUFFER_FULL))
		return B_UNHANDLED_INTERRUPT;
		
	if (atomic_get(&sIgnoreInterrupts)) {
		TRACE(("ps2_interrupt: ignoring, ctrl 0x%02x (%s)\n", ctrl, (ctrl & PS2_STATUS_MOUSE_DATA) ? "mouse" : "keyb"));
		return B_HANDLED_INTERRUPT;
	}
	
	data = ps2_read_data();

	TRACE(("ps2_interrupt: ctrl 0x%02x, data 0x%02x (%s)\n", ctrl, data, (ctrl & PS2_STATUS_MOUSE_DATA) ? "mouse" : "keyb"));

	if (ctrl & PS2_STATUS_MOUSE_DATA) {
		uint8 idx = 0;
		dev = &ps2_device[PS2_DEVICE_MOUSE + idx];
	} else {
		dev = &ps2_device[PS2_DEVICE_KEYB];
	}
	
	return ps2_dev_handle_int(dev, data);
}


//	#pragma mark - driver interface


status_t
init_hardware(void)
{
	return B_OK;
}


const char **
publish_devices(void)
{
	return NULL;
}


device_hooks *
find_device(const char *name)
{
	return NULL;
}


status_t
init_driver(void)
{
	status_t status;

	status = get_module(B_ISA_MODULE_NAME, (module_info **)&gIsa);
	if (status < B_OK)
		goto err_1;

	sKbcSem = create_sem(1, "ps/2 keyb ctrl");

	gDeviceOpenSemaphore = create_sem(1, "ps/2 open");
	if (gDeviceOpenSemaphore < B_OK)
		goto err_2;

	status = ps2_dev_init();

	status = ps2_service_init();

	status = install_io_interrupt_handler(INT_PS2_KEYBOARD, &ps2_interrupt, NULL, 0);
	if (status)
		goto err_3;
	
	status = install_io_interrupt_handler(INT_PS2_MOUSE,    &ps2_interrupt, NULL, 0);
	if (status)
		goto err_4;

	{
		uint8 d;
		status_t res;
		
		res = ps2_get_command_byte(&d);
		dprintf("ps2_get_command_byte: res 0x%08x, d 0x%02x\n", res, d);
		
		d |= PS2_BITS_TRANSLATE_SCANCODES | PS2_BITS_KEYBOARD_INTERRUPT | PS2_BITS_AUX_INTERRUPT;
		d &= ~(PS2_BITS_KEYBOARD_DISABLED | PS2_BITS_MOUSE_DISABLED);
		
		res = ps2_set_command_byte(d);
		dprintf("ps2_set_command_byte: res 0x%08x, d 0x%02x\n", res, d);
	}

	ps2_service_handle_device_added(&ps2_device[PS2_DEVICE_KEYB]);
	ps2_service_handle_device_added(&ps2_device[PS2_DEVICE_MOUSE]);
	if (sMultiplexingActive) {
		ps2_service_handle_device_added(&ps2_device[PS2_DEVICE_MOUSE + 1]);
		ps2_service_handle_device_added(&ps2_device[PS2_DEVICE_MOUSE + 2]);
		ps2_service_handle_device_added(&ps2_device[PS2_DEVICE_MOUSE + 3]);
	}
	
	//goto err_5;	
	
	
	return B_OK;

err_5:
	remove_io_interrupt_handler(INT_PS2_MOUSE,    &ps2_interrupt, NULL);
err_4:	
	remove_io_interrupt_handler(INT_PS2_KEYBOARD, &ps2_interrupt, NULL);
err_3:
	ps2_service_exit();
	ps2_dev_exit();
	delete_sem(gDeviceOpenSemaphore);
	delete_sem(sKbcSem);
err_2:
	put_module(B_ISA_MODULE_NAME);
err_1:
	return B_ERROR;
}


void
uninit_driver(void)
{
	remove_io_interrupt_handler(INT_PS2_MOUSE,    &ps2_interrupt, NULL);
	remove_io_interrupt_handler(INT_PS2_KEYBOARD, &ps2_interrupt, NULL);
	ps2_service_exit();
	ps2_dev_exit();
	delete_sem(gDeviceOpenSemaphore);
	delete_sem(sKbcSem);
	put_module(B_ISA_MODULE_NAME);
}
