#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "cpad.h"
#include "vfb2_user.h"

int ufb = -1;
int fb_file = -1;
char fb_device[16];
caddr_t fb = (caddr_t) -1;
int cpad = -1;
char cpad_default[] = "/dev/usb/cpad0";
char *cpad_device = cpad_default;

#define CPAD_FB_SIZE		118784

void leave(int i)
{
	if (cpad >= 0)
		close(cpad);
	if (fb != (caddr_t) -1)
		munmap(fb, CPAD_FB_SIZE);
	if (fb_file >= 0)
		close(fb_file);
	if (ufb >= 0)
		close(ufb);
	exit(i);
}

void init_fb(void)
{
	struct vfb2_mode mode = { 240, 160, 24, FB_VISUAL_TRUECOLOR };
	__u32 vsize = CPAD_FB_SIZE;
	int node;

	if ((ufb = open("/proc/" UVFB2_DEVICE, O_RDWR)) < 0) {
		perror("can not open userfb");
		leave(-1);
	}
	if (ioctl(ufb, UVFB2_ADD_MODE, &mode)) {
		perror("can not add mode");
		leave(-1);
	}
	if (ioctl(ufb, UVFB2_VMEM_SIZE, &vsize)) {
		perror("can not init fb");
		leave(-1);
	}
	if (ioctl(ufb, UVFB2_NODE, &node)) {
		perror("can not get node");
		leave(-1);
	}
	sprintf(fb_device, "/dev/fb%i", node);
	printf("registered frame buffer device: %s\n", fb_device);

	printf("waiting for udev to create fb device...\n");
	sleep(1);

	if ((fb_file = open(fb_device, O_RDWR)) < 0) {
		perror("can not open frame buffer");
		leave(-1);
	}
	fb = mmap(0, CPAD_FB_SIZE, PROT_READ, MAP_SHARED, fb_file, 0);
	if (fb == (caddr_t) -1) {
		perror("can not mmap frame buffer");
		leave(-1);
	}
}

#define CPAD_BUFFER_SIZE	(160*30)
unsigned char buffer1[CPAD_BUFFER_SIZE+1] = { };
unsigned char buffer2[CPAD_BUFFER_SIZE+1] = { };
unsigned char *buffer = buffer1;
unsigned char *old_buffer = buffer2;
unsigned char read_buffer[32];

void switch_buffers(void)
{
	unsigned char *tmp;

	tmp = buffer;
	buffer = old_buffer;
	old_buffer = tmp;
}

void inline cpad_write(void *data, size_t size)
{
	unsigned char *datap = data;
	unsigned char cmd = datap[0];
	unsigned char backup;
	int res;
	int remain = size-1;

	if (!size)
		return;

	do {
		backup = datap[0];
		datap[0] = cmd;

		res = write(cpad, datap, remain+1);
		if (res <= 0) {
			perror("cpad write error");
			//leave(-1);
		}

		datap[0] = backup;

		remain -= res-1;
		datap += res-1;
	} while (remain);

	if (read(cpad, read_buffer, 32) < 0) {
		perror("cpad read error");
		//leave(-1);
	}
}

void set_cursor(int pos)
{
	unsigned char set_cursor[] = { CSRW_1335, pos & 0xff, pos >> 8 };

	cpad_write(&set_cursor, sizeof(set_cursor));
}

int start = 0;
int length = CPAD_BUFFER_SIZE;

int idle_count;

void send_buffer(void)
{
	unsigned char c;

	if (length == 0) {
		idle_count++;
		return;
	} else
		idle_count = 0;

	c = buffer[start];
	buffer[start] = MWRITE_1335;

	set_cursor(start);
	cpad_write(buffer+start, length+1);

	buffer[start] = c;
}

int only_changed = 1;

void compare_buffers(void)
{
	int i;

	if (!only_changed)
		return;

	for (i=1; i<=CPAD_BUFFER_SIZE; i++)
		if (buffer[i] != old_buffer[i]) {
			start = i-1;
			goto get_length;
		}
	length = 0;
	return;

get_length:
	for (i=CPAD_BUFFER_SIZE+1; i>start; i--)
		if (buffer[i] != old_buffer[i]) {
			length = i - start;
			break;
		}
}

int dither2[2][2] = 	{{1, 3},
			 {4, 2}};

int dither4[4][4] = 	{{ 1,  9,  3, 11},
			 {13,  5, 15,  7},
			 { 4, 12,  2, 10},
			 {16,  8, 14,  6}};

int dither8[8][8] = 	{{ 1, 33,  9, 41,  3, 35, 11, 43},
			 {49, 17, 57, 25, 51, 19, 59, 27},
			 {13, 45,  5, 37, 15, 47,  7, 39},
			 {61, 29, 53, 21, 63, 31, 55, 23},
			 { 4, 36, 12, 44,  2, 34, 10, 42},
			 {52, 20, 60, 28, 50, 18, 58, 26},
			 {16, 48,  8, 40, 14, 46,  6, 38},
			 {64, 32, 56, 24, 62, 30, 54, 22}};

int maxgrey = 255*10;

inline int dither(int x, int y, int grey, int dither)
{
	switch (dither) {
	case 0:	return (2*grey > maxgrey) ? 0 : 1;
	case 1:	return (5*grey > dither2[x%2][y%2]*maxgrey) ? 0 : 1;
	case 2:	return (17*grey > dither4[x%4][y%4]*maxgrey) ? 0 : 1;
	case 3:	return (65*grey > dither8[x%8][y%8]*maxgrey) ? 0 : 1;
	}
}

int brightness = 200;
int dither_mode = 3;
int invert = 0;

#define set_bit(bit, data) do { data |= 1 << (bit); } while(0)
#define clear_bit(bit, data) do { data &= ~(1 << (bit)); } while(0)
#define min(x, y) (((x)<(y)) ? (x) : (y))
#define max(x, y) (((x)<(y)) ? (y) : (x))

void fill_buffer(void)
{
	unsigned char *vmem = fb;
	int i, grey, black;

	for (i=0; i<240*160; i++) {
		grey = 3*vmem[0] + 6*vmem[1] + vmem[2];
		grey = min((grey*brightness)/100, maxgrey);

		black = dither(i%240, i/240, grey, dither_mode);
		if (invert)
			black = !black;

		if (black)
			set_bit(7-i%8, buffer[i/8+1]);
		else
			clear_bit(7-i%8, buffer[i/8+1]);

		vmem += 3;
	}
}

void send_image(void)
{
	fill_buffer();
	compare_buffers();
	send_buffer();
	switch_buffers();
}

/* the cpad can do ca. 30 fps (max) */
#define MAX_FRAMERATE 30
#define CPAD_SEND_DELAY (1000000/MAX_FRAMERATE)
#define DELAY_PER_BYTE (((float)CPAD_SEND_DELAY/(float)CPAD_BUFFER_SIZE))
int framerate = 20;
int idle_rate = 4;

inline int calc_delay()
{
	int delay, rate;
	float send_time;

	/* calculate time to send image */
	send_time = DELAY_PER_BYTE*length;

	rate = (idle_count > framerate) ? idle_rate : framerate;
	delay = max(0, 1000000/rate - (int)send_time);

	return delay;
}

#define PROGRAM_DESCRIPTION \
"cpadfb - puts a frame buffer on the cPad\n\
\n\
Syntax:\n\
	%s [-d device] [-f framerate] [-b brightness] [-i 0/1]\n\
		[--dither 0-3] [--onlychanged 0/1] [--idle-rate rate]\n\
\n\
Description:\n\
	-d device - cpad character device (default: /dev/usb/cpad0)\n\
	-f framerate - value from 1-30 (default: 20)\n\
	-b brightness - value from 0-10000 (default: 200)\n\
	-i 0/1 - invert screen (default: 0)\n\
	--dither 0-3 - dithering mode (default: 3)\n\
	--onlychanged 0/1 - send only changed parts (default: 1)\n\
	--idle-rate rate - framerate when idle (default: 4)\n\
\n"

void command_line(int argc, char **argv)
{
	int i;

	for (i=1; i<argc; i++) {
		if ((!strcmp(argv[i], "-d")) && (i<argc-1)) {
			cpad_device = argv[++i];
		} else if ((!strcmp(argv[i], "-f")) && (i<argc-1)) {
			if (sscanf(argv[++i], "%i", &framerate) != 1)
				goto error;
			if ((framerate < 1) || (framerate > 30))
				goto error;
		} else if (!strcmp(argv[i], "-b")) {
			if (sscanf(argv[++i], "%i", &brightness) != 1)
				goto error;
			if ((brightness < 0) || (brightness > 10000))
				goto error;
		} else if (!strcmp(argv[i], "-i")) {
			if (sscanf(argv[++i], "%i", &invert) != 1)
				goto error;
			if ((invert != 0) && (invert != 1))
				goto error;
		} else if (!strcmp(argv[i], "--dither")) {
			if (sscanf(argv[++i], "%i", &dither_mode) != 1)
				goto error;
			if ((dither_mode < 0) || (dither_mode > 3))
				goto error;
		} else if (!strcmp(argv[i], "--onlychanged")) {
			if (sscanf(argv[++i], "%i", &only_changed) != 1)
				goto error;
			if ((only_changed != 0) && (only_changed != 1))
				goto error;
		} else if (!strcmp(argv[i], "--idle-rate")) {
			if (sscanf(argv[++i], "%i", &idle_rate) != 1)
				goto error;
			if ((idle_rate < 1) || (idle_rate > 30))
				goto error;
		} else
			goto error;
	}

	idle_rate = min(idle_rate, framerate);

	return;
error:
	printf(PROGRAM_DESCRIPTION, argv[0]);
	leave(-2);
}

int main(int argc, char **argv)
{
	command_line(argc, argv);
	init_fb();
	if ((cpad = open(cpad_device, O_RDWR)) < 0) {
		perror("can not open cpad");
		leave(-1);
	}

	printf("frame buffer ready! press <ctrl>-c to remove frame buffer.\n");
	while (1) {
		send_image();
		usleep(calc_delay());
	}

	return 0;
}
