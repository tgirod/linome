#include "linome.h"

void lp_initialize()
{
	//initialize usb
	if(libusb_init(NULL)!=0){
		fprintf(stderr,"Unable to initialize usb\n");
		exit(1);
	} else {
		printf("usb initialized\n");
	}

	//find the device
	device = libusb_open_device_with_vid_pid(NULL, ID_VENDOR, ID_PRODUCT);
	if (device == NULL) {
		fprintf(stderr,"Unable to find the launchpad\n");
		exit(1);
	} else {
		printf("launchpad found\n");
	}

	//claim the device
	if(libusb_claim_interface(device, 0) != 0) {
		fprintf(stderr,"Unable to claim the launchpad (try modprobe -r snd-usb-audio)\n");
		exit(1);
	} else {
		printf("launchpad claimed\n");
	}

	// reset launchpad
	lp_message(0xB0, 0x00, 0x00);

	// set double-buffer mode
	lp_message(0xB0, 0x00, 0x31);

	// init mutex
	pthread_mutex_init(&led_mutex, NULL);
	pthread_mutex_init(&dest_mutex, NULL);
	
	// set OSC's default destination
    dest = lo_address_new("localhost", "9999");
	
	// init OSC server
	osc = lo_server_new(NULL, lp_error_handler);
	if (!osc) {
		fprintf(stderr, "Unable to start OSC server\n");
		exit(1);
	} else {
		printf("OSC server listening on port %d\n", lo_server_get_port(osc));
	}

    lo_server_add_method(osc, "/grid/led/set", "iii", lp_grid_led_set, NULL);
    lo_server_add_method(osc, "/scene/led/set", "ii", lp_scene_led_set, NULL);
    lo_server_add_method(osc, "/ctrl/led/set", "ii", lp_ctrl_led_set, NULL);

    lo_server_add_method(osc, "/sys/host", "s", lp_sys_host, NULL);
    lo_server_add_method(osc, "/sys/port", "i", lp_sys_port, NULL);
}

void lp_terminate()
{
	// terminate osc
	lo_server_free(osc);

	// terminate USB
	libusb_release_interface(device,0);
	libusb_close(device);
	libusb_exit(NULL);
	
	// terminate mutex
	pthread_mutex_destroy(&led_mutex);
}

void lp_send(int length)
{
	// FIXME add a loop in order to resume data send in case of interrupt ?
	int transmitted;

	// send the data
	libusb_interrupt_transfer(
			device,
			EP_OUT,
			output,
			length,
			&transmitted,
			0);

	if (transmitted != length) {
		fprintf(stderr, "transmitted %d bytes instead of %d\n", transmitted, length);
	}
}

void lp_message(unsigned int data0, unsigned int data1, unsigned int data2)
{
	output[0] = data0;
	output[1] = data1;
	output[2] = data2;
	lp_send(3);
}

void lp_update()
{
	// swap buffers (thread-safe part)
	pthread_mutex_lock(&led_mutex);
	memcpy(&led[!buffer], &led[buffer], 80);
	buffer = !buffer;
	pthread_mutex_unlock(&led_mutex);

	// prepare data to be sent
	output[0] = 0x92;
	memcpy(&output[1], &led[!buffer][0][0], 80);
	output[81] = 0xB0;
	output[82] = 0x00;
	output[83] = 0x31 + 3*buffer;

	// send data
	lp_send(84);
}

void lp_set_grid(int row, int col, red_t red, green_t green)
{
	pthread_mutex_lock(&led_mutex);
	if (0<=row && row<8 && 0<=col && col<8) {
		led[buffer][row][col] = red|green;
	}
	pthread_mutex_unlock(&led_mutex);
}

void lp_set_scene(int row, red_t red, green_t green)
{
	pthread_mutex_lock(&led_mutex);
	if (0<=row && row<8) {
		led[buffer][8][row] = red|green;
	}
	pthread_mutex_unlock(&led_mutex);
}

void lp_set_ctrl(int col, red_t red, green_t green)
{
	pthread_mutex_lock(&led_mutex);
	if (0<=col && col<8) {
		led[buffer][9][col] = red|green;
	}
	pthread_mutex_unlock(&led_mutex);
}

/* 
 * OSC server
 */

void lp_error_handler(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

int lp_grid_led_set(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	int row = argv[0]->i;
	int col = argv[1]->i;
	int color = argv[2]->i; 
	if (color) {
		lp_set_grid(row, col, red_full, green_full);
	} else {
		lp_set_grid(row, col, red_off, green_off);
	}
	return 0;
}

int lp_scene_led_set(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	int row = argv[0]->i;
	int color = argv[1]->i;
	if (color) {
		lp_set_scene(row, red_full, green_full);
	} else {
		lp_set_scene(row, red_off, green_off);
	}
	return 0;
}

int lp_ctrl_led_set(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
    int col = argv[0]->i;
    int color = argv[1]->i;
    if (color) {
    	lp_set_ctrl(col, red_full, green_full);
	} else {
		lp_set_ctrl(col, red_off, green_off);
	}
	return 0;
}

int lp_sys_host(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	char *host = &argv[0]->s;
	const char *port = lo_address_get_port(dest);

	pthread_mutex_lock(&dest_mutex);
	lo_address_free(dest);
	dest = lo_address_new(host,port);
	pthread_mutex_unlock(&dest_mutex);

	printf("linome is now sending to %s %s\n", lo_address_get_hostname(dest), lo_address_get_port(dest));
	return 0;
}

int lp_sys_port(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	const char *host = lo_address_get_hostname(dest);
	char *port = malloc(6);
	if (!port) {
		fprintf(stderr, "lp_port_handler: malloc error\n");
		return 1;
	}
	snprintf(port, 6, "%d", argv[0]->i);

	pthread_mutex_lock(&dest_mutex);
	lo_address_free(dest);
	dest = lo_address_new(host,port);
	pthread_mutex_unlock(&dest_mutex);
	free(port);

	printf("linome is now sending to %s %s\n", lo_address_get_hostname(dest), lo_address_get_port(dest));
	return 0;
}

/*
 * threads
 */

void *lp_refresh_routine(void *arg)
{
	printf("refresh thread running\n");
	while (run) {
		lp_update();
	}
	return NULL;
}

void *lp_osc_routine(void *arg)
{
	printf("osc thread running\n");
	while (run) {
		lo_server_recv(osc);
	}
	return NULL;
}

void *lp_launchpad_routine(void *arg)
{
	int size,i,x,y,pressed;
	char *path;
	unsigned char prefix = NOTE;
	lo_message msg;

	printf("launchpad thread running\n");
	while (run) {
		libusb_interrupt_transfer(device, EP_IN, input, MAX_PACKET_SIZE, &size, 0);
		i = 0;
		while (i<size) {
			if (input[i] == NOTE || input[i] == CTRL) {
				prefix = input[i++];
			}
			msg = lo_message_new();
			if (prefix == NOTE) {
				// grid or scene event
				x = input[i] / 0x10;
				y = input[i++] % 0x10;
				pressed = input[i++] == 127;
				
				if (y < 8) {
					// grid
					path = "/grid/key";
					lo_message_add_int32(msg,x);
					lo_message_add_int32(msg,y);
					lo_message_add_int32(msg,pressed);
				} else {
					// scene
					path = "/scene/key";
					lo_message_add_int32(msg,x);
					lo_message_add_int32(msg,pressed);
				}
			} else {
				// ctrl event
				y = input[i++] - 0x68;
				pressed = input[i++] == 127;
				path = "/ctrl/key";

				lo_message_add_int32(msg,y);
				lo_message_add_int32(msg,pressed);
			}
			pthread_mutex_lock(&dest_mutex);
			lo_send_message(dest,path,msg);
			pthread_mutex_unlock(&dest_mutex);
			lo_message_free(msg);
		}
	}
	return NULL;
}

int main(int argc, char** argv)
{
	lp_initialize();
	
	// launch threads
	run = 1;
	pthread_create(&refresh_thread, NULL, lp_refresh_routine, NULL);
	pthread_create(&osc_thread, NULL, lp_osc_routine, NULL);
	pthread_create(&launchpad_thread, NULL, lp_launchpad_routine, NULL);

	// wait for threads
	pthread_join(refresh_thread, NULL);
	pthread_join(osc_thread, NULL);
	pthread_join(launchpad_thread, NULL);
	
	// end things gracefully
	lp_terminate();
	return 0;
}
