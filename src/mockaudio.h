#ifndef QRQ_MOCK_AUDIO_H
#define QRQ_MOCK_AUDIO_H

int open_dsp(char *device);
int write_audio(int fd, const int *samples, int size);
int close_audio(int fd);

#endif
