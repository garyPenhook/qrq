/* 
Copyright (C) 2011  Fabian Kurz, DJ1YFK
 
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

PulseAudio specific functions and includes.

*/

#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/error.h>

extern long samplerate;
extern void  *dsp_fd;

short int *buf = 0;
size_t bufsize = 0;
size_t bufpos = 0;
static int buffer_failed = 0;

void *open_dsp (char *dummy) {
	static int opened = 0;

	/* with PA we only open the device once and then leave it
	   opened */
	if (opened) {
		return dsp_fd;
	}

	/* The Sample format to use */
	static pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 8000,
		.channels = 1
	};
	ss.rate = samplerate;
	pa_simple *s = NULL;
	int error;

	if (!(s = pa_simple_new(NULL, "qrq", PA_STREAM_PLAYBACK, NULL, 
				"playback", &ss, NULL, NULL, &error))) {
	        fprintf(stderr, "pa_simple_new() failed: %s\n", 
				pa_strerror(error));
	}

	if (s == NULL) {
		return NULL;
	}

	opened = 1;
	return s;
}

/* actually just puts samples into the buffer that is played at the end 
(close_audio) */
void write_audio (void *bla, int *in, int size) {
	short int *new_buf;
	size_t sample_count;
	size_t required;
	size_t new_size;
	size_t i;

	(void) bla;
	if (in == NULL || size < 0 || buffer_failed) {
		buffer_failed = 1;
		return;
	}

	sample_count = (size_t) size / sizeof(*in);
	if (sample_count > SIZE_MAX - bufpos) {
		buffer_failed = 1;
		return;
	}
	required = bufpos + sample_count;
	if (required > bufsize) {
		new_size = bufsize ? bufsize : 16384;
		while (new_size < required) {
			if (new_size > SIZE_MAX / 2) {
				new_size = required;
				break;
			}
			new_size *= 2;
		}
		if (new_size > SIZE_MAX / sizeof(*buf)) {
			buffer_failed = 1;
			return;
		}
		new_buf = realloc(buf, new_size * sizeof(*buf));
		if (new_buf == NULL) {
			buffer_failed = 1;
			return;
		}
		buf = new_buf;
		bufsize = new_size;
	}
	for (i=0; i < sample_count; i++) {
		buf[bufpos + i] = (short int) in[i];
	}	
	bufpos = required;
}

void close_audio (void *s) {
	int e;
	if (s == NULL || buffer_failed) {
		bufpos = 0;
		buffer_failed = 0;
		return;
	}
	if (bufpos != 0 && pa_simple_write(s, buf, bufpos * sizeof(*buf), &e) < 0) {
		fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(e));
	}
	else if (pa_simple_drain(s, &e) < 0) {
		fprintf(stderr, "pa_simple_drain() failed: %s\n", pa_strerror(e));
	}
	bufpos = 0;
}

