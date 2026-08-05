/* 
Copyright (C) 2006-2007  Fabian Kurz
 
$Id$

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
Street, Fifth Floor, Boston, MA  02110-1301, USA.

OSS specific functions and includes.

*/

#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <ncurses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

extern long samplerate;

int open_dsp (char * device) {
	int tmp;
	int fd;
	
	if ((fd = open(device, O_WRONLY, 0)) == -1) {
		endwin();
		perror(device);
		exit(EXIT_FAILURE);
	}

	tmp = AFMT_S16_NE; 
	if (ioctl(fd, SNDCTL_DSP_SETFMT, &tmp)==-1) {
		endwin();
		perror("SNDCTL_DSP_SETFMT");
		exit(EXIT_FAILURE);
	}

	if (tmp != AFMT_S16_NE) {
		endwin();
		fprintf(stderr, "Cannot switch to AFMT_S16_NE\n");
		exit(EXIT_FAILURE);
	}
  
	tmp = 2;	/* 2 channels, stereo */
	if (ioctl(fd, SNDCTL_DSP_CHANNELS, &tmp)==-1) {
		endwin();
		perror("SNDCTL_DSP_CHANNELS");
		exit(EXIT_FAILURE);
	}

	if (tmp != 2) {
		endwin();
		fprintf(stderr, "No stereo mode possible :(.\n");
		exit(EXIT_FAILURE);
	}

	if (ioctl(fd, SNDCTL_DSP_SPEED, &samplerate)==-1) {
		endwin();
		perror("SNDCTL_DSP_SPEED");
		exit(EXIT_FAILURE);
	}
return fd;
}

int write_audio(int fd, const int *samples, int size) {
	const unsigned char *next = (const unsigned char *)samples;
	size_t remaining;

	if (fd < 0 || samples == NULL || size < 0) {
		errno = EINVAL;
		return -1;
	}
	remaining = (size_t)size;
	while (remaining != 0) {
		ssize_t written = write(fd, next, remaining);

		if (written > 0) {
			next += (size_t)written;
			remaining -= (size_t)written;
			continue;
		}
		if (written < 0 && errno == EINTR) {
			continue;
		}
		if (written == 0) {
			errno = EIO;
		}
		return -1;
	}
	return 0;
}

int close_audio(int fd) {
	return close(fd);
}
