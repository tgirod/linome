#ifndef LINOME_H
#define LINOME_H

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <lo/lo.h>

// launchpad identifiers
#define ID_VENDOR  0x1235
#define ID_PRODUCT 0x000E

// usb endpoints
#define EP_IN      ( LIBUSB_ENDPOINT_IN  | 1)
#define EP_OUT     ( LIBUSB_ENDPOINT_OUT | 2)

#define MAX_PACKET_SIZE 8
#define NOTE 0x90
#define CTRL 0xB0

typedef enum red {
	red_off		= 0x00,
	red_low		= 0x01,
	red_medium	= 0x02,
	red_full	= 0x03 
} red_t;

typedef enum green {
	green_off		= 0x00,
	green_low		= 0x10,
	green_medium	= 0x20,
	green_full		= 0x30 
} green_t;

unsigned char led[2][10][8] = {0}; 	// leds state : two buffers of 10 lines of 8 leds
pthread_mutex_t led_mutex; // avoid conflicting access to the led array
int buffer = 0;					// buffer currently in write mode

struct libusb_device_handle *device = NULL;		// usb device
unsigned char output[123] = {0};				// output buffer
unsigned char input[8] = {0};					// input buffer

lo_server osc;
int run; 				// should we keep running ?
lo_address dest; 		// destination of sent OSC messages
pthread_mutex_t dest_mutex;

pthread_t refresh_thread, osc_thread, launchpad_thread;

void lp_initialize();
void lp_terminate();
void lp_send(int length);
void lp_message(unsigned int data0, unsigned int data1, unsigned int data2);
void lp_update();

void lp_set_grid(int row, int col, red_t red, green_t green);
void lp_set_scene(int row, red_t red, green_t green);
void lp_set_ctrl(int col, red_t red, green_t green);

void lp_error_handler(int num, const char *msg, const char *path);

int lp_grid_led_set(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
int lp_scene_led_set(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
int lp_ctrl_led_set(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

int lp_grid_led_all(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
int lp_scene_led_all(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
int lp_ctrl_led_all(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

int lp_sys_host(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
int lp_sys_port(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

// update leds forever
void *lp_refresh_routine(void *arg);
// listen to incoming osc messages and convert them to led updates
void *lp_osc_routine(void *arg);
// listen to incoming launchpad messages and convert them to osc messages
void *lp_launchpad_routine(void *arg);

int main(int argc, char** argv);

#endif
