#include "mockaudio.h"

int open_dsp(char *device) {
	(void)device;
	return 1;
}

int write_audio(int fd, const int *samples, int size) {
	return fd == 1 && (samples != 0 || size == 0) && size >= 0 ? 0 : -1;
}

int close_audio(int fd) {
	return fd == 1 ? 0 : -1;
}
