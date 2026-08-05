/* 
qrq - High speed morse trainer, similar to the DOS classic "Rufz"

Copyright (C) 2006-2025  Fabian Kurz and contributors

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

*/ 
#if WIN32
#define WIN_THREADS
typedef int AUDIO_HANDLE;
#endif

#ifndef WIN_THREADS
#include <pthread.h>			/* CW output will be in a separate thread */
#endif
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>				/* basename */
#include <ctype.h>
#include <time.h> 
#include <limits.h> 			/* PATH_MAX */
#include <stdint.h>
#include <stdarg.h>

#ifndef PATH_MAX				/* Not defined e.g. on GNU/hurd */
#define PATH_MAX 4096 
#endif

#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>			/* mkdir */
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#ifdef WIN32
#include <windows.h>
#endif

#define PI 3.14159265358979323846

#define SILENCE 0		/* Waveforms for the tone generator */
#define SINE 1
#define SAWTOOTH 2
#define SQUARE 3

#define CAPITALS_ON   1
#define CAPITALS_OFF  0

#define CALL_MAX    28    /* maximum allowed length of a call/word. limit to 28 so we can fit word + correction into the window */

#ifndef DESTDIR
#	define DESTDIR "/usr"
#endif

#ifndef VERSION
#  define VERSION "0.0.0"
#endif

#ifdef CA
#include "coreaudio.h"
typedef void *AUDIO_HANDLE;
#endif

#ifdef OSS
#include "oss.h"
#define write_audio(x, y, z) write(x, y, z)
#define close_audio(x) close(x)
typedef int AUDIO_HANDLE;
#endif

#ifdef PA
#include "pulseaudio.h"
typedef void *AUDIO_HANDLE;
#endif

#include "score.h"

/* callsign array will be dynamically allocated */
static char **calls = NULL;
static size_t calls_allocated = 0;

static const char *codetable[] = {
".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",".---",
"-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",
".--","-..-","-.--","--..","-----",".----","..---","...--","....-",".....",
"-....", "--...","---..","----."};

/* List of available callbase files. Probably no need to do dynamic memory allocation for that list.... */

static char cblist[100][PATH_MAX];

static char mycall[15]="DJ5CW";			/* mycall. will be read from qrqrc */
static char dspdevice[PATH_MAX]="/dev/dsp";	/* will also be read from qrqrc */
static int score = 0;					/* qrq score */
#ifdef WIN_THREADS
static volatile LONG sending_complete = 1;
#else
static int sending_complete = 1;
static pthread_mutex_t sending_complete_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static int callnr = 0;					/* nr of actual call in attempt */
static int initialspeed=200;			/* initial speed. to be read from file*/
static int mincharspeed=0;				/* min. char. speed, below: farnsworth*/
static int speed=200;					/* current speed in cpm */
static int maxspeed=0;
static int speedupstep=10;				/* speed increase after correct copies */
static int speeddownstep=10;			/* speed decrease after incorrect copies */
static int stoponerror=0;               /* after an error, stop and wait for 2nd enter */
static int freq=800;					/* current cw sidetone freq */
static int errornr=0;					/* number of errors in attempt */
static int p=0;							/* position of cursor, relative to x */
static int status=1;					/* 1= attempt, 2=config */
static int mode=1;						/* 0 = overwrite, 1 = insert */
static int j=0;							/* counter etc. */
static int constanttone=0;              /* if 1 don't change the pitch */
static int ctonefreq=800;               /* if constanttone=1 use this freq */
static int f6=0;						/* f6 = 1: allow unlimited repeats */
static int fixspeed=0;					/* keep speed fixed, regardless of err*/
static int unlimitedattempt=0;			/* attempt with all calls  of the DB */
static int sessionlength=50;				/* calls per standard practice session */
static int attemptvalid=1;				/* 1 = not using any "cheats" */
static unsigned long int nrofcalls=0;	
static int toplist_own=0;               /* show only own call on toplist */
static int call_maxlen = 0;				/* maximum length of a callsign/word from current database */

long samplerate=44100;
static long long_i;
static int waveform = SINE;				/* waveform: (0 = none) */
static char wavename[10]="Sine    ";	/* Name of the waveform */
static double edge=2.0;						/* rise/fall time in milliseconds */
static int ed;							/* risetime, normalized to samplerate */

static short buffer[88200];
static int *full_buf = NULL;
static size_t full_buf_capacity = 0;
static size_t full_bufpos = 0;

AUDIO_HANDLE dsp_fd;

static int display_toplist(void);
static int calc_score (char * realcall, char * input, int speed, char * output, int f6pressed);
static int update_score(void);
static int show_error (char * realcall, char * wrongcall); 
static int clear_display(void);
static int add_to_toplist(char * mycall, int score, int maxspeed);
static int read_config(void);
static int save_config(void);
static int tonegen(int freq, int length, int waveform);
static void *morse(void * arg); 
static int add_to_buf(const void *data, size_t size);
static int readline(WINDOW *win, int y, int x, char *line, int capitals, int len); 
static void thread_fail (int j);
static int check_toplist (void);
static int find_files (void);
static int statistics (void);
static int read_callbase (void);
static void find_callbases(void);
static void select_callbase (void);
static void help (void);
static void callbase_dialog(void);
static void parameter_dialog(void);
static int clear_parameter_display(void);
static void update_parameter_dialog(void);
static void start_summary_file(void);
static void close_summary_file(void);
static int append_summary(const char *format, ...);
static int validchar(int c);
static void free_calls(void);
static int copy_file(const char *source_path, const char *destination_path);
static int write_file_atomic(const char *path, const void *data, size_t length);

/* The audio worker and ncurses input loop run concurrently.  Do not access
 * this state directly: a plain int (or volatile int) is a data race. */
static void set_sending_complete(int complete) {
#ifdef WIN_THREADS
	InterlockedExchange(&sending_complete, complete ? 1 : 0);
#else
	pthread_mutex_lock(&sending_complete_mutex);
	sending_complete = complete;
	pthread_mutex_unlock(&sending_complete_mutex);
#endif
}

static int is_sending_complete(void) {
#ifdef WIN_THREADS
	return InterlockedCompareExchange(&sending_complete, 0, 0) != 0;
#else
	int complete;
	pthread_mutex_lock(&sending_complete_mutex);
	complete = sending_complete;
	pthread_mutex_unlock(&sending_complete_mutex);
	return complete;
#endif
}


#ifdef WIN_THREADS
HANDLE cwthread;
#else
pthread_t cwthread;				/* thread for CW output, to enable
								   keyboard reading at the same time */
pthread_attr_t cwattr;
#endif

char rcfilename[PATH_MAX]="";			/* filename and path to qrqrc */
char tlfilename[PATH_MAX]="";			/* filename and path to toplist */
char cbfilename[PATH_MAX]="";			/* filename and path to callbase */
char sumfilepath[PATH_MAX]="";			/* path where to save summary files for each attempt */

char destdir[PATH_MAX]="";

char *summary = NULL;                    /* detailed attempt summary, saved in a file */
size_t summary_capacity = 0;
char summary_scr_fmt[255]="";           /* format string for a single summary score line */
char summary_hdr_fmt[255]="";           /* format string for the summary header line */
size_t s_pos = 0;                        /* Position within summary */
int summary_failed = 0;

/* create windows */
WINDOW *top_w;					/* actual score					*/
WINDOW *mid_w;					/* callsign history/mistakes	*/
WINDOW *conf_w;					/* parameter config display	*/
WINDOW *bot_w;					/* user input line				*/
WINDOW *inf_w;					/* info window for param displ	*/
WINDOW *right_w;				/* highscore list/settings		*/


int main (int argc, char *argv[]) {
	(void)argv;

  /* if built as osx bundle set DESTDIR to Resources dir of bundle */
#ifdef OSX_BUNDLE
  char tempdir[PATH_MAX]="";
  char* p_slash = strrchr(argv[0], '/');
  strncpy(tempdir, argv[0], p_slash - argv[0]);
  p_slash = strrchr(tempdir, '/');
  if(p_slash != NULL) {
    strncpy(destdir, tempdir, p_slash - tempdir);
  }
  strcat(destdir, "/Resources");
#else
  strcpy(destdir, DESTDIR);
#endif

	char abort = 0;
	char tmp[CALL_MAX + 1]="";
	char input[CALL_MAX + 1]="";
	int i=0,j=0,k=0;						/* counter etc. */
	int attempt_limit;
	char previouscall[CALL_MAX + 1]="";
	int previousfreq = 0;
	int f6pressed=0;

	if (argc > 1) {
		help();
	}
	
	(void) initscr();
	cbreak();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	scrollok(stdscr, FALSE);
	
	printw("qrq v%s - Copyright (C) 2006-2021 Fabian Kurz, DJ5CW\n", VERSION);
	printw("This is free software, and you are welcome to redistribute it\n");
	printw("under certain conditions (see COPYING).\n");

	refresh();

	/* search for 'toplist', 'qrqrc' and callbase.qcb and put their locations
	 * into tlfilename, rcfilename, cbfilename */
	find_files();

	/* check if the toplist is in the suitable format. as of 0.0.7, each line
	 * is 31 characters long, with the added time stamp */
	check_toplist();

	/* buffer for audio */
	for (long_i=0;long_i<88200;long_i++) {
		buffer[long_i]=0;
	}
	
	/* random seed from time */
	srand( (unsigned) time(NULL) ); 

#ifndef WIN_THREADS
	/* Initialize cwthread. We have to wait for the cwthread to finish before
	 * the next cw output can be made, this will be done with pthread_join */
	pthread_attr_init(&cwattr);
	pthread_attr_setdetachstate(&cwattr, PTHREAD_CREATE_JOINABLE);
#endif
	
	/****** Reading configuration file ******/
	printw("\nReading configuration file qrqrc \n");
	read_config();

	attemptvalid = 1;
	if (f6 || fixspeed || unlimitedattempt || sessionlength != 50) {
		attemptvalid = 0;	
	}

	/****** Reading callsign database ******/
	printw("\nReading callsign database... ");
	nrofcalls = read_callbase();

	printw("done. %ld calls read.\n\n", nrofcalls);
	printw("Press any key to continue...");

	refresh();
	getch();

	erase();
	refresh();

	top_w = newwin(4, 60, 0, 0);
	mid_w = newwin(17, 60, 4, 0);
	conf_w = newwin(17, 60, 4, 0);
	bot_w = newwin(3, 60, 21, 0);
	inf_w = newwin(3, 60, 21, 0);
	right_w = newwin(24, 20, 0, 60);

	werase(top_w);
	werase(mid_w);
	werase(conf_w);
	werase(bot_w);
	werase(inf_w);
	werase(right_w);

	keypad(bot_w, TRUE);
	keypad(mid_w, TRUE);
	keypad(conf_w, TRUE);

#ifdef WIN_THREADS
	cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,"QRQ",0, NULL);
#else
	/* no need to join here, this is the first possible time CW is sent */
	pthread_create(&cwthread, NULL, & morse, (void *) "QRQ");
#endif

/* very outter loop */
while (1) {	

/* status 1 = running an attempt of 50 calls */	
while (status == 1) {
	box(top_w,0,0);
	box(conf_w,0,0);
	box(mid_w,0,0);
	box(bot_w,0,0);
	box(inf_w,0,0);
	box(right_w,0,0);
	wattron(top_w,A_BOLD);
	mvwaddstr(top_w,1,1, "QRQ v");
	mvwaddstr(top_w,1,6, VERSION);
	wattroff(top_w, A_BOLD);
	mvwaddstr(top_w,1,11, " by Fabian Kurz, DJ5CW           ");
	mvwaddstr(top_w,2,1, "Homepage and Toplist: https://fkurz.net/ham/qrq.html"
					"     ");

	clear_display();
	wattron(mid_w,A_BOLD);
	mvwaddstr(mid_w,1,1, "Usage:");
	mvwaddstr(mid_w,10,2, "F6                          F10       ");
	wattroff(mid_w, A_BOLD);
	if (unlimitedattempt) {
		mvwaddstr(mid_w,2,2, "After entering your callsign, all random callsigns");
	}
	else {
		mvwprintw(mid_w,2,2, "After entering your callsign, %d random callsigns",
				sessionlength);
	}
	mvwaddstr(mid_w,3,2, "from a database will be sent. After each callsign,");
	mvwaddstr(mid_w,4,2, "enter what you have heard. If you copied correctly,");
	mvwaddstr(mid_w,5,2, "full points are credited and the speed increases by");
	mvwprintw(mid_w,6,2, "+%d LpM; errors decrease it by %d LpM.",
			speedupstep, speeddownstep);
	mvwaddstr(mid_w,7,2, "fraction of the points, depending on the number of");
	mvwaddstr(mid_w,8,2, "errors is credited.");
	mvwaddstr(mid_w,10,2, "F6 repeats a callsign once, F10 quits.");
	mvwaddstr(mid_w,11,2, "F8 toggles showing only your results in the toplist.");
	mvwaddstr(mid_w,12,2, "Settings can be changed with F5 (or in qrqrc).");
#ifndef WIN32
	mvwaddstr(mid_w,14,2, "Score statistics (requires gnuplot) with F7.");
#endif

	wattron(right_w,A_BOLD);
	mvwaddstr(right_w,1, 6, "Toplist");
	wattroff(right_w,A_BOLD);

	display_toplist();

	p=0;						/* cursor to start position */
	wattron(bot_w,A_BOLD);
	mvwaddstr(bot_w, 1, 1, "Please enter your callsign:                      ");
	wattroff(bot_w,A_BOLD);
	
	wrefresh(top_w);
	wrefresh(mid_w);
	wrefresh(bot_w);
	wrefresh(right_w); 
	
	/* reset */
	maxspeed = errornr = score = 0;
	speed = initialspeed;
	
	/* prompt for own callsign */
	i = readline(bot_w, 1, 30, mycall, CAPITALS_ON, 8);

	/* F5 -> Configure sound */
	if (i == 5) {
		parameter_dialog();
		break;
	} 
	/* F6 -> play test CW */
	else if (i == 6) {
		freq = constanttone ? ctonefreq : 800;
#ifdef WIN_THREADS
		 WaitForSingleObject(cwthread,INFINITE);
		 CloseHandle(cwthread);
		 cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,"VVVTEST",0, NULL);
#else
		pthread_join(cwthread, NULL);
		j = pthread_create(&cwthread, NULL, &morse, (void *) "VVVTEST");	
		thread_fail(j);
#endif
		break;
	}
	else if (i == 7) {
#ifndef WIN32
		statistics();
#endif
		break;
	}
	else if (i == 8) {
		toplist_own = toplist_own == 1 ? 0 : 1;
		display_toplist();
		break;
	}

	if (strlen(mycall) == 0) {
		strcpy(mycall, "NOCALL");
	}
	else if (strlen(mycall) > 7) {		/* cut excessively long calls */
		mycall[7] = '\0';
	}
	
	clear_display();
	wrefresh(mid_w);
	
	/* update toplist (highlight may change) */
	display_toplist();

	mvwprintw(top_w,1,1,"                                      ");
	mvwprintw(top_w,2,1,"                                               ");
	mvwprintw(top_w,1,1,"Callsign:");
	wattron(top_w,A_BOLD);
	mvwprintw(top_w,1,11, "%s", mycall);
	wattroff(top_w,A_BOLD);
	update_score();
	wrefresh(top_w);


	/* Reread callbase */
	nrofcalls = read_callbase();

	/****** send a configured number of calls, ask for input, score ******/
    start_summary_file();
	if (nrofcalls > (unsigned long)INT_MAX) {
		attempt_limit = INT_MAX;
	}
	else if (unlimitedattempt || sessionlength > (int)nrofcalls) {
		attempt_limit = (int)nrofcalls;
	}
	else {
		attempt_limit = sessionlength;
	}

    for (callnr=1; callnr <= attempt_limit; callnr++) {
		/* Make sure to wait for the cwthread of the previous callsign, if
		 * necessary. */
#ifdef WIN_THREADS
		WaitForSingleObject(cwthread,INFINITE);
		CloseHandle(cwthread);
#else
		pthread_join(cwthread, NULL);
#endif	
		/* select an unused callsign from the calls-array */
		do {
			i = (int) ((float) nrofcalls*rand()/(RAND_MAX+1.0));
		} while (calls[i] == NULL);

		/* output frequency handling a) random b) fixed */
		if ( constanttone == 0 ) {
				/* random freq, fraction of samplerate */
				freq = (int) (samplerate/(50+(40.0*rand()/(RAND_MAX+1.0))));
		}
		else { /* fixed frequency */
				freq = ctonefreq;
		}

		mvwprintw(bot_w,1,1,"                                      ");
		mvwprintw(bot_w, 1, 1, "%3d/%s", callnr, unlimitedattempt ? "-" : "");
		if (!unlimitedattempt) {
			wprintw(bot_w, "%d", attempt_limit);
		}
		wrefresh(bot_w);	
		tmp[0]='\0';

		/* starting the morse output in a separate process to make keyboard
		 * input and echoing at the same time possible */
		
		set_sending_complete(0);
#ifdef WIN_THREADS
		cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,calls[i],0, NULL);
#else
		j = pthread_create(&cwthread, NULL, morse, calls[i]);	
		thread_fail(j);		
#endif
		
		f6pressed=0;

		while (!abort && (j = readline(bot_w, 1, 8, input, CAPITALS_ON, CALL_MAX)) > 4) {/* F5..F10 pressed */

			switch (j) {
				case 6:		/* repeat call */
				if (f6pressed && (f6 == 0)) {
					continue;
				}
				f6pressed=1;
				/* wait for old cwthread to finish, then send call again */
			
#ifdef WIN_THREADS
			WaitForSingleObject(cwthread,INFINITE);
			CloseHandle(cwthread);
			set_sending_complete(0);
			cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,calls[i],0, NULL);
#else
			pthread_join(cwthread, NULL);
			set_sending_complete(0);
			j = pthread_create(&cwthread, NULL, &morse, calls[i]);	
			thread_fail(j);
#endif	
					break; /* 6*/
				case 7:		/* repeat _previous_ call */
					if (callnr > 1) {
						k = freq;
						freq = previousfreq;
#ifdef WIN_THREADS
			WaitForSingleObject(cwthread,INFINITE);
			CloseHandle(cwthread);
			set_sending_complete(0);
			cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,previouscall,0, NULL);
			WaitForSingleObject(cwthread,INFINITE);
			CloseHandle(cwthread);
#else
			pthread_join(cwthread, NULL);
			set_sending_complete(0);
			j = pthread_create(&cwthread, NULL, &morse, previouscall);	
			thread_fail(j);
			pthread_join(cwthread, NULL);
#endif	
						/* NB: We must wait for the CW thread before
						 * we set the freq back -- this blocks keyboard
						 * input, but in this case it shouldn't matter */
						freq = k;
					}
					break;
				case 10:	/* abort attempt */
					abort = 1;
					continue;
					break;
			}
					
		}

		
		if (abort) {
			abort = 0;
			input[0]='\0';
			break;
		}
		
		tmp[0]='\0';
		score += calc_score(calls[i], input, speed, tmp, f6pressed);
		update_score();
		if (strcmp(tmp, "-")) {			/* made an error */
				show_error(calls[i], tmp);
                if (stoponerror)
                    getch();
		}
		input[0]='\0';
		strncpy(previouscall, calls[i], CALL_MAX);
		previousfreq = freq;
		calls[i] = NULL;
	}

    close_summary_file();

	/* attempt is over, send AR */
	callnr = 0;
	
#ifdef WIN_THREADS
		 WaitForSingleObject(cwthread,INFINITE);
		 CloseHandle(cwthread);
		 cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,"+",0, NULL);
#else
		pthread_join(cwthread, NULL);
		j = pthread_create(&cwthread, NULL, &morse, (void *) "+");	
		thread_fail(j);
#endif
	
	add_to_toplist(mycall, score, maxspeed);
	
	curs_set(0);
	wattron(bot_w,A_BOLD);
	mvwprintw(bot_w,1,1, "Attempt finished. Press any key to continue!");
	wattroff(bot_w,A_BOLD);
	wrefresh(bot_w);
	getch();
	mvwprintw(bot_w,1,1, "                                            ");
	curs_set(1);

	
} /* while (status == 1) */


} /* very outter loop */

	getch();
	endwin();
	delwin(top_w);
	delwin(bot_w);
	delwin(mid_w);
	delwin(right_w);
	getch();
	return 0;
}



/* (formerly status == 2). Change parameters */

void parameter_dialog (void) {

int j = 0;


update_parameter_dialog();

while ((j = getch()) != 0) {

	switch ((int) j) {
		case '+':							/* rise/falltime */
			if (edge <= 9.0) {
				edge += 0.1;
			}
			break;
		case '-':
			if (edge > 0.1) {
				edge -= 0.1;
			}
			break;
		case 'w':							/* change waveform */
			waveform = ((waveform + 1) % 3)+1;	/* toggle 1-2-3 */
			break;
		case 'k':							/* constanttone */
			if (ctonefreq >= 160) {
				ctonefreq -= 10;
			}
			else {
					constanttone = 0;
			}
			break;
		case 'l':
			if (constanttone == 0) {
				constanttone = 1;
			}
			else if (ctonefreq < 1600) {
				ctonefreq += 10;
			}
			break;
		case '0':
			if (constanttone == 1) {
				constanttone = 0;
			}
			else {
				constanttone = 1;
			}
			break;
		case 'f':
				f6 = (f6 ? 0 : 1);
			break;
		case 's':
				fixspeed = (fixspeed ? 0 : 1);
			break;
		case 't':
				stoponerror = (stoponerror ? 0 : 1);
			break;
		case 'u':
				unlimitedattempt = (unlimitedattempt ? 0 : 1);
			break;
		case '[':
			if (sessionlength > 5) {
				sessionlength -= 5;
			}
			unlimitedattempt = 0;
			break;
		case ']':
			if (sessionlength <= INT_MAX - 5) {
				sessionlength += 5;
			}
			unlimitedattempt = 0;
			break;
		case KEY_UP: 
			initialspeed += 10;
			break;
		case KEY_DOWN:
			if (initialspeed > 10) {
				initialspeed -= 10;
			}
			break;
		case KEY_RIGHT:
			mincharspeed += 10;
			break;
		case KEY_LEFT:
			if (mincharspeed > 10) {
				mincharspeed -= 10;
			}
			break;
		case KEY_PPAGE:
			speedupstep += 2;
			break;
		case KEY_NPAGE:
			if (speedupstep >= 4) {
				speedupstep -= 2;
			}
			break;
		case ',':
			if (speeddownstep >= 4) {
				speeddownstep -= 2;
			}
			break;
		case '.':
			speeddownstep += 2;
			break;
		case 'c':
			readline(conf_w, 6, 25, mycall, CAPITALS_ON, 8);
			if (strlen(mycall) == 0) {
				strcpy(mycall, "NOCALL");
			}
			else if (strlen(mycall) > 7) {	/* cut excessively long calls */
				mycall[7] = '\0';
			}
			p=0;							/* cursor position */
			break;
#ifdef OSS
		case 'e':
			readline(conf_w, 12, 25, dspdevice, CAPITALS_OFF, 14);
			if (strlen(dspdevice) == 0) {
				strcpy(dspdevice, "/dev/dsp");
			}
			p=0;							/* cursor position */
			break;
#endif
		case 'd':							/* go to database browser */
			if (!callnr) {					/* Only allow outside of attempt */
				curs_set(1);
				callbase_dialog();
			}
			break;
		case KEY_F(2):
			save_config();	
			mvwprintw(conf_w,15,23, "  Config saved!");
			wrefresh(conf_w);
#ifdef WIN32
			Sleep(1000);
#else
			sleep(1);	
#endif
			break;
		case KEY_F(6):
			freq = constanttone ? ctonefreq : 800;
#ifdef WIN_THREADS
		 WaitForSingleObject(cwthread,INFINITE);
		 CloseHandle(cwthread);
		 cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,"TESTING",0, NULL);
#else
		pthread_join(cwthread, NULL);
		j = pthread_create(&cwthread, NULL, &morse, (void *) "TESTING");	
		thread_fail(j);
#endif
			break;
		case KEY_F(10):
		case KEY_F(3):
			curs_set(1);
			clear_parameter_display();
			wrefresh(conf_w);
			/* restore old windows */
			touchwin(mid_w);
			touchwin(bot_w);
			wrefresh(mid_w);
			wrefresh(bot_w);
			return;
	}

	speed = initialspeed;

	attemptvalid = 1;
	if (f6 || fixspeed || unlimitedattempt || sessionlength != 50) {
		attemptvalid = 0;	
	}

	update_parameter_dialog();

} /* while 1 (return only by F3/F10) */

} /* parameter_dialog */


/* update_parameter_dialog 
 * repaints the whole config/parameter screen (F5) */


void update_parameter_dialog (void) {

	clear_parameter_display();
	switch (waveform) {
		case SINE:
			strcpy(wavename, "Sine    ");
			break;
		case SAWTOOTH:
			strcpy(wavename, "Sawtooth");
			break;
		case SQUARE:
			strcpy(wavename, "Square  ");
			break;
	}

	mvwaddstr(inf_w,1,1, "                                                         ");
	curs_set(0);
	wattron(conf_w,A_BOLD);
	mvwaddstr(conf_w,1,1, "Configuration:          Value                Change");
	mvwprintw(conf_w,15,2, "F6                   F2                F10");
	wattroff(conf_w, A_BOLD);
	mvwprintw(conf_w,2,2, "Initial Speed:         %3d CpM / %3d WpM" 
					"    up/down", initialspeed, initialspeed/5);
	mvwprintw(conf_w,3,2, "Min. character Speed:  %3d CpM / %3d WpM" 
					"    left/right", mincharspeed, mincharspeed/5);
	mvwprintw(conf_w,4,2, "Speed stepping:        +%3d/-%-3d CpM   "
					" PgUp/PgDn ,/.", speedupstep, speeddownstep);
	mvwprintw(conf_w,5,2, "CW rise/falltime (ms): %1.1f           " 
					"       +/-", edge);
	mvwprintw(conf_w,6,2, "Callsign:              %-14s" 
					"       c", mycall);
	mvwprintw(conf_w,7,2, "CW pitch (0 = random): %-4d"
					"                 k/l or 0", (constanttone)?ctonefreq : 0);
	mvwprintw(conf_w,8,2, "CW waveform:           %-8s"
					"             w", wavename);
	mvwprintw(conf_w,9,2, "Allow unlimited F6*:   %-3s"
					"                  f", (f6 ? "yes" : "no"));
	mvwprintw(conf_w,10,2, "Fixed CW speed*:       %-3s"
					"                  s", (fixspeed ? "yes" : "no"));
	mvwprintw(conf_w,11,2, "Session calls*:         %-5s"
					"                [ / ] or u",
			unlimitedattempt ? "all" : "");
	if (!unlimitedattempt) {
		wprintw(conf_w, "%d", sessionlength);
	}
	if (!callnr) {
		mvwprintw(conf_w,12,2, "Callsign database:     %-15s"
					"      d (%ld)", basename(cbfilename),nrofcalls);
	}
	mvwprintw(conf_w,13,2, "Stop on error:         %-3s"
                    "                  t", (stoponerror ? "yes" : "no"));
#ifdef OSS
	mvwprintw(conf_w,14,2, "DSP device:            %-15s"
					"      e", dspdevice);
#endif
	mvwprintw(conf_w,15,4, ": Play CW sample");
	mvwprintw(conf_w,15,25, ": Save config");
	mvwprintw(conf_w,15,44, ": Exit");
	mvwprintw(inf_w,1,1, "          * Makes scores ineligible for toplist");
	wrefresh(conf_w);
	wrefresh(inf_w);
	

} /* update_parameter_dialog */




void callbase_dialog (void) {

	clear_parameter_display();

	wattron(conf_w,A_BOLD);
	mvwaddstr(conf_w,1,1, "Change Callsign Database");
	wattroff(conf_w,A_BOLD);
#if WIN32
	mvwprintw(conf_w,3,1, ".qcb files found:");
#else
	mvwprintw(conf_w,3,1, ".qcb files found (in %s/share/qrq/ and ~/.qrq/):",destdir);
#endif

	/* populate cblist */	
	find_callbases();
	/* selection dialog */
	select_callbase();
	wrefresh(conf_w);

	nrofcalls = read_callbase();


	return;	/* back to config menu */
}














/* reads a callsign etc. in *win at y/x and writes it to *line */

static int readline(WINDOW *win, int y, int x, char *line, int capitals, int len) {
	int c;						/* character we read */
	int i=0;
	size_t line_len;

	if (strlen(line) == 0) {p=0;}	/* cursor to start if no call in buffer */
	
	if (mode == 1) { 
		mvwaddstr(win,1,55,"INS");
	}
	else {
		mvwaddstr(win,1,55,"OVR");
	}

	mvwaddstr(win,y,x,line);
	wmove(win,y,x+p);
	wrefresh(win);
	curs_set(TRUE);
	
	while (1) {
		c = wgetch(win);
		if (c == '\n' && is_sending_complete())
			break;
		line_len = strlen(line);

		if (validchar(c) && line_len < (size_t)len) {

            // accept - as / for German keyboards (and other layouts where /
            // requires pressing shift)
            if (c == '-') {
                c = '/';
            }

			line[line_len + 1] = '\0';
			if (capitals) {
				c = toupper(c);
			}
			if (mode == 1) {						/* insert */
				for(i = (int)line_len; i > p; i--) {	/* move all chars by one */
					line[i] = line[i-1];
				}
			} 
			line[p]=c;						/* insert into gap */
			p++;
		}
		else if ((c == KEY_BACKSPACE || c == 127 || c == 9 || c == 8)
						&& p != 0) {					/* BACKSPACE */
			for (i=p-1; i < (int)line_len; i++) {
				line[i] =  line[i+1];
			}
			p--;
		}
		else if (c == KEY_DC && p < (int) strlen(line)) {	/* DELETE */
			for (i=p; i < (int)line_len; i++) {
				line[i] =  line[i+1];
			}
		}
		else if (c == KEY_LEFT && p != 0) {
			p--;	
		}
		else if (c == KEY_RIGHT && p < (int)line_len) {
			p++;
		}
		else if (c == KEY_HOME) {
			p = 0;
		}
		else if (c == KEY_END) {
			p = (int)line_len;
		}
		else if (c == KEY_IC) {						/* INS/OVR */
			if (mode == 1) { 
				mode = 0; 
				mvwaddstr(win,1,55,"OVR");
			}
			else {
				mode = 1;
				mvwaddstr(win,1,55,"INS");
			}
		}
		else if (c == KEY_PPAGE && callnr && !attemptvalid) {
			speed += 5;
			update_score();
			wrefresh(top_w);
		}
		else if (c == KEY_NPAGE && callnr && !attemptvalid) {
			if (speed > 20) speed -= 5;
			update_score();
			wrefresh(top_w);
		}
		else if (c == KEY_F(5)) {
			parameter_dialog();
		}
		else if (c == KEY_F(6)) {
			return 6;
		}
		else if (c == KEY_F(7)) {
			return 7;
		}
		else if (c == KEY_F(8)) {
			return 8;
		}
		else if (c == KEY_F(10)) {				/* quit */
			if (callnr) {						/* quit attempt only */
				return 10;
			} 
			/* else: quit program */
			endwin();
			printf("Thanks for using 'qrq'!\nYou can submit your"
					" highscore to http://fkurz.net/ham/qrqtop.php\n");
			/* make sure that no more output is running, then send 73 & quit */
			speed = 200; freq = 800;
#ifdef WIN_THREADS
		 WaitForSingleObject(cwthread,INFINITE);
		 CloseHandle(cwthread);
		 cwthread = (HANDLE) _beginthreadex( NULL, 0, morse,"73",0, NULL);
		 WaitForSingleObject(cwthread,INFINITE);
		 CloseHandle(cwthread);
#else
		pthread_join(cwthread, NULL);
		j = pthread_create(&cwthread, NULL, &morse, (void *) "73");	
		thread_fail(j);
			/* make sure the cw thread doesn't die with the main thread */
			/* Exit the whole main thread */
			pthread_join(cwthread, NULL);
#endif
			exit(0);
		}

		for (int p = 0; p <= len; p++) {	
			mvwaddstr(win,y,x+p," ");
		}
		mvwaddstr(win,y,x,line);
		wmove(win,y,x+p);
		wrefresh(win);
	}
	curs_set(FALSE);
	return 0;
}

/* Read toplist and diplay first 10 entries */
static int display_toplist (void) {
	FILE * fh;
	int i = 0;
	int first = 1;
	char tmp[35]="";
	if ((fh = fopen(tlfilename, "a+")) == NULL) {
		endwin();
		fprintf(stderr, "Couldn't read or create file '%s'!", tlfilename);
		exit(EXIT_FAILURE);
	}
	rewind(fh);				/* a+ -> end of file, we want the beginning */
	(void) fgets(tmp, 34, fh);		/* first line not used */
	while ((feof(fh) == 0) && i < 21) {
		if (fgets(tmp, 34, fh) != NULL) {
			tmp[17]='\0';
			if (toplist_own) {
				if (strstr(tmp, mycall)) {   /* only show own call */
					mvwaddstr(right_w,i+2, 2, tmp);
					i++;
				}
			}
			else {
				if (strstr(tmp, mycall)) {		/* highlight own call */
					wattron(right_w, A_BOLD);
				}
				mvwaddstr(right_w,i+2, 2, tmp);
				i++;
				wattroff(right_w, A_BOLD);
			}
		}
	}
	// delete remaining lines
	while (i < 21) {
		if (first) {
			mvwaddstr(right_w,i+2, 2, "  *** end ***   ");
			first = 0;
		}
		else {
			mvwaddstr(right_w,i+2, 2, "                ");
		}
		i++;
	}
	fclose(fh);
	wrefresh(right_w);
	return 0;
}

/* calculate score depending on number of errors and speed.
 * writes the correct call and entered call with highlighted errors to *output
 * and returns the score for this call
 *
 * in training modes (unlimited attempts, f6, fixed speed), no points.
 * */
static int calc_score (char * realcall, char * input, int spd, char * output, int f6pressed) {
	struct qrq_score_state state = {
		.speed = speed,
		.maxspeed = maxspeed,
		.error_count = errornr,
		.fixed_speed = fixspeed,
		.attempt_valid = attemptvalid,
		.speed_up_step = speedupstep,
		.speed_down_step = speeddownstep,
	};
	int score = qrq_score_attempt(&state, realcall, input, spd, output, CALL_MAX + 1);

	speed = state.speed;
	maxspeed = state.maxspeed;
	errornr = state.error_count;

	if (append_summary(summary_scr_fmt, realcall, input, output, spd, spd/5,
			score, f6pressed ? '*' : ' ') != 0) {
		summary_failed = 1;
	}

    return score;
}

static int append_summary(const char *format, ...) {
	va_list args;
	va_list args_copy;
	char *resized;
	size_t required;
	size_t new_capacity;
	int written;

	va_start(args, format);
	va_copy(args_copy, args);
	written = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (written < 0 || s_pos > SIZE_MAX - (size_t)written - 1) {
		va_end(args_copy);
		return -1;
	}
	required = s_pos + (size_t)written + 1;
	if (required > summary_capacity) {
		new_capacity = summary_capacity == 0 ? 1024 : summary_capacity;
		while (new_capacity < required) {
			if (new_capacity > SIZE_MAX / 2) {
				new_capacity = required;
				break;
			}
			new_capacity *= 2;
		}
		resized = realloc(summary, new_capacity);
		if (resized == NULL) {
			va_end(args_copy);
			return -1;
		}
		summary = resized;
		summary_capacity = new_capacity;
	}
	if (vsnprintf(summary + s_pos, summary_capacity - s_pos, format, args_copy) != written) {
		va_end(args_copy);
		return -1;
	}
	va_end(args_copy);
	s_pos += (size_t)written;
	return 0;
}

static void start_summary_file (void) {
	int row_format_len;
	int header_format_len;

	row_format_len = snprintf(summary_scr_fmt, sizeof(summary_scr_fmt),
			"%%-%ds %%-%ds %%-%ds %%3d %%3d %%5d %%c\r\n", call_maxlen + 2,
			call_maxlen + 2, call_maxlen + 2);
	header_format_len = snprintf(summary_hdr_fmt, sizeof(summary_hdr_fmt),
			"%%-%ds %%-%ds %%-%ds %%-3s %%-3s %%-5s %%s\r\n", call_maxlen + 2,
			call_maxlen + 2, call_maxlen + 2);
	if (row_format_len < 0 || (size_t)row_format_len >= sizeof(summary_scr_fmt) ||
			header_format_len < 0 || (size_t)header_format_len >= sizeof(summary_hdr_fmt)) {
		summary_failed = 1;
		return;
	}

	s_pos = 0;
	summary_failed = 0;
	if (summary != NULL) {
		summary[0] = '\0';
	}
	if (append_summary("QRQ attempt by %s.\r\n\r\n", mycall) != 0 ||
			append_summary(summary_hdr_fmt, "Sent call", "Input", "Difference", "CpM",
					"WpM", "Score", "F6") != 0) {
		summary_failed = 1;
		return;
	}
	for (int i = 0; i < (3 * (call_maxlen + 2) + 30); i++) {
		if (append_summary("-") != 0) {
			summary_failed = 1;
			return;
		}
	}
	if (append_summary("\r\n") != 0) {
		summary_failed = 1;
	}
}

static void close_summary_file (void) {
    FILE *fh;
    time_t t;
    struct tm *tmp;
    char time_fmt[256];
    char *filename;
    size_t filename_len;
    size_t path_len;
    size_t call_len;
    size_t time_len;

    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL) {
        return;
    }

    if (strftime(time_fmt, sizeof(time_fmt), "%Y%m%d_%H%M", tmp) == 0) {
        return;
    }

	if (summary_failed || append_summary("\r\n") != 0 ||
			append_summary("Score: %d, Max. speed (CpM/WpM): %d / %d\r\nSaved at: %s\r\n",
				score, maxspeed, maxspeed/5, time_fmt) != 0) {
		fprintf(stderr, "Unable to create complete attempt summary.\n");
		return;
	}

	path_len = strlen(sumfilepath);
	call_len = strlen(mycall);
	time_len = strlen(time_fmt);
	if (path_len > SIZE_MAX - call_len - time_len - 7) {
		fprintf(stderr, "Summary filename is too long.\n");
		return;
	}
	filename_len = path_len + call_len + time_len + 7;
	filename = malloc(filename_len);
	if (filename == NULL) {
		fprintf(stderr, "Out of memory while creating summary filename.\n");
		return;
	}
	(void)snprintf(filename, filename_len, "%s/%s-%s.txt", sumfilepath, mycall, time_fmt);

	if ((fh = fopen(filename, "w")) == NULL) {
		printf("Unable to open summary file (%s)!\r\n", filename);
		free(filename);
		return;
	}

	if (fwrite(summary, 1, s_pos, fh) != s_pos) {
		fprintf(stderr, "Unable to write summary file (%s)!\n", filename);
	}
	fclose(fh);
	
    for (int i = 12; i <= 15; i++) {
        mvwprintw(mid_w,i,2, "                                                         ");
    }

	mvwprintw(mid_w,13,1, " Written detailed summary of this attempt to:");
	mvwprintw(mid_w,14,2, "%s", filename);
    wrefresh(mid_w);
	free(filename);

}

/* print score, current speed and max speed to window */
static int update_score(void) {
	mvwaddstr(top_w,1,20, "Score:                         ");
	mvwaddstr(top_w,2,20, "Speed:     CpM/    WpM, Max:    /  ");
	if (attemptvalid) {
		mvwprintw(top_w, 1, 27, "%6d", score);	
	}
	else {
		mvwprintw(top_w, 1, 27, "[training mode]");	
	}
	mvwprintw(top_w, 2, 27, "%3d", speed);	
	mvwprintw(top_w, 2, 35, "%3d", speed/5);	
	mvwprintw(top_w, 2, 49, "%3d", maxspeed);	
	mvwprintw(top_w, 2, 54, "%3d", maxspeed/5);	
	wrefresh(top_w);
	return 0;
}

/* display the correct callsign and what the user entered, with mistakes
 * highlighted. */
static int show_error (char * realcall, char * wrongcall) {
	int x=2;
	int y = errornr;
	int i;

	// when call_maxlen <= CALL_MAX/2, we are showing the errors in two columns, otherwise just one.
	int max_nr_err = call_maxlen <= CALL_MAX/2 ? 30 : 15;   
	int max_disp_len = call_maxlen <= CALL_MAX/2 ? CALL_MAX/2 : CALL_MAX;

	// cut entered call if it's longer than what we can display
	if (strlen(wrongcall) > (size_t)max_disp_len) {
		wrongcall[max_disp_len] = '\0';
	}

	/* Screen is full of errors. Remove them and start at the beginning */
	if (errornr >= max_nr_err) {	
		for (i=1;i<16;i++) {
			mvwaddstr(mid_w,i,2,"                                        "
							 "          ");
		}
		errornr = y = 1;
	}

	/* Move to second column after 15 errors if applicable */	
	if (max_nr_err == 30 && errornr > 15) {
		x=30; y = (errornr % 16)+1;
	}

	if (call_maxlen <= CALL_MAX / 2) {
		mvwprintw(mid_w, y, x, "%-14s %-14s", realcall, wrongcall);
	}
	else {
		mvwprintw(mid_w, y, x, "%-28s %-28s", realcall, wrongcall);
	}
	wrefresh(mid_w);		
	return 0;
}

/* clear error display */
static int clear_display(void) {
	int i;
	for (i=1;i<16;i++) {
		mvwprintw(mid_w,i,1,"                                 "
										"                        ");
	}
	return 0;
}

/* clear parameter display */
static int clear_parameter_display(void) {
	int i;
	for (i=1;i<16;i++) {
		mvwprintw(conf_w,i,1,"                                 "
										"                        ");
	}
	return 0;
}


/* write entry into toplist at the right place 
 * going down from the top of the list until the score in the current line is
 * lower than the score made. then */

static int add_to_toplist(char *mycall, int score, int maxspeed) {
	FILE *fh = NULL;
	char *old_data = NULL;
	char *new_data = NULL;
	char insertline[35];
	char score_text[7];
	char *endptr;
	size_t file_size;
	size_t insert_offset;
	size_t line_length;
	size_t offset;
	long file_length;
	long timestamp;
	long listed_score;
	int written;
	int result = -1;

	/* For the training modes */
	if (score == 0) {
		return 0;
	}

	timestamp = (long)time(NULL);
	written = snprintf(insertline, sizeof(insertline),
			"%-10.10s%6d %3d %10ld", mycall, score, maxspeed, timestamp);
	if (written != 31) {
		fprintf(stderr, "Unable to format toplist entry.\n");
		return -1;
	}

	if ((fh = fopen(tlfilename, "rb")) == NULL) {
		fprintf(stderr, "Unable to open toplist file %s!\n", tlfilename);
		return -1;
	}

	if (fseek(fh, 0, SEEK_END) != 0 || (file_length = ftell(fh)) < 0) {
		fprintf(stderr, "Unable to determine size of toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	file_size = (size_t)file_length;
	if (fseek(fh, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Unable to rewind toplist file %s!\n", tlfilename);
		goto cleanup;
	}

	old_data = malloc(file_size == 0 ? 1 : file_size);
	if (old_data == NULL) {
		fprintf(stderr, "Out of memory while reading toplist.\n");
		goto cleanup;
	}
	if (file_size != 0 && fread(old_data, 1, file_size, fh) != file_size) {
		fprintf(stderr, "Unable to read toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	if (fclose(fh) != 0) {
		fh = NULL;
		fprintf(stderr, "Unable to close toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	fh = NULL;

	/* Toplists use fixed-width entries: 32 bytes with LF, 33 with CRLF. */
	line_length = file_size >= 32 && old_data[31] == '\r' ? 33 : 32;
	if (file_size != 0 && file_size % line_length != 0) {
		fprintf(stderr, "Invalid toplist format in %s!\n", tlfilename);
		goto cleanup;
	}
	if (line_length == 33) {
		memcpy(insertline + 31, "\r\n", 3);
	}
	else {
		memcpy(insertline + 31, "\n", 2);
	}

	if (file_size > SIZE_MAX - line_length) {
		fprintf(stderr, "Toplist file is too large to update.\n");
		goto cleanup;
	}
	new_data = malloc(file_size + line_length);
	if (new_data == NULL) {
		fprintf(stderr, "Out of memory while updating toplist.\n");
		goto cleanup;
	}

	insert_offset = file_size;
	for (offset = 0; offset < file_size; offset += line_length) {
		memcpy(score_text, old_data + offset + 10, 6);
		score_text[6] = '\0';
		errno = 0;
		listed_score = strtol(score_text, &endptr, 10);
		if (errno == 0 && *endptr == '\0' && score > listed_score) {
			insert_offset = offset;
			break;
		}
	}

	memcpy(new_data, old_data, insert_offset);
	memcpy(new_data + insert_offset, insertline, line_length);
	memcpy(new_data + insert_offset + line_length, old_data,
			file_size - insert_offset);

	if (write_file_atomic(tlfilename, new_data, file_size + line_length) != 0) {
		fprintf(stderr, "Unable to atomically update toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	result = 0;

cleanup:
	if (fh != NULL) {
		fclose(fh);
	}
	free(old_data);
	free(new_data);
	return result;
}


/* Read config file 
 *
 * TODO contains too much copypasta. write proper function to parse a key=value
 *
 * */

static int read_config (void) {
	FILE *fh;
	char tmp[80]="";
	int i=0;
	int k=0;
	int line=0;

	if ((fh = fopen(rcfilename, "r")) == NULL) {
		endwin();
		fprintf(stderr, "Unable to open config file %s!\n", rcfilename);
		exit(EXIT_FAILURE);
	}

	while ((feof(fh) == 0) && (fgets(tmp, 80, fh) != NULL)) {
		i=0;
		line++;
		tmp[strlen(tmp)-1]='\0';

		/* find callsign, speed etc. 
		 * only allow if the lines are beginning at zero, so stuff can be
		 * commented out easily; return value if strstr must point to tmp*/
		if(tmp == strstr(tmp,"callsign=")) {
			while (isalnum(tmp[i] = toupper(tmp[9+i]))) {
				i++;
			}
			tmp[i]='\0';
			if (strlen(tmp) < 8) {				/* empty call allowed */
				strcpy(mycall,tmp);
				printw("  line  %2d: callsign: >%s<\n", line, mycall);
			}
			else {
				printw("  line  %2d: callsign: >%s< too long. "
								"Using default >%s<.\n", line, tmp, mycall);
			}
		}
		else if (tmp == strstr(tmp,"initialspeed=")) {
			while (isdigit(tmp[i] = tmp[13+i])) {
				i++;
			}
			tmp[i]='\0';
			i = atoi(tmp);
			if (i > 9) {
				initialspeed = speed = i;
				printw("  line  %2d: initial speed: %d\n", line, initialspeed);
			}
			else {
				printw("  line  %2d: initial speed: %d invalid (range: 10..oo)."
								" Using default %d.\n",line,  i, initialspeed);
			}
		}
		else if (tmp == strstr(tmp,"mincharspeed=")) {
			while (isdigit(tmp[i] = tmp[13+i])) {
				i++;
			}
			tmp[i]='\0';
			if ((i = atoi(tmp)) > 0) {
				mincharspeed = i;
				printw("  line  %2d: min.char.speed: %d\n", line, mincharspeed);
			} /* else ignore */
		}
		else if (tmp == strstr(tmp,"speedstep=")) {
			while (isgraph(tmp[i] = tmp[strlen("speedstep=")+i])) {
				i++;
			}
			tmp[i]='\0';
			if ((i = atoi(tmp)) > 0) {
				speedupstep = speeddownstep = i;
				printw("  line  %2d: legacy speed step: %d\n", line, i);
			}
			else {
				printw("  line  %2d: speed step: >%s< invalid. "
								"Using defaults +%d/-%d.\n", line, tmp,
								speedupstep, speeddownstep);
			}
		}
		else if (tmp == strstr(tmp,"speedupstep=")) {
			while (isdigit((unsigned char)(tmp[i] = tmp[12+i]))) {
				i++;
			}
			tmp[i]='\0';
			if ((i = atoi(tmp)) > 0) {
				speedupstep = i;
				printw("  line  %2d: speed-up step: %d\n", line, speedupstep);
			}
		}
		else if (tmp == strstr(tmp,"speeddownstep=")) {
			while (isdigit((unsigned char)(tmp[i] = tmp[14+i]))) {
				i++;
			}
			tmp[i]='\0';
			if ((i = atoi(tmp)) > 0) {
				speeddownstep = i;
				printw("  line  %2d: speed-down step: %d\n", line, speeddownstep);
			}
		}
		else if (tmp == strstr(tmp,"dspdevice=")) {
			while (isgraph(tmp[i] = tmp[10+i])) {
				i++;
			}
			tmp[i]='\0';
			if (strlen(tmp) > 1) {
				strcpy(dspdevice,tmp);
				printw("  line  %2d: dspdevice: >%s<\n", line, dspdevice);
			}
			else {
				printw("  line  %2d: dspdevice: >%s< invalid. "
								"Using default >%s<.\n", line, tmp, dspdevice);
			}
		}
		else if (tmp == strstr(tmp, "risetime=")) {
			while (isdigit(tmp[i] = tmp[9+i]) || ((tmp[i] = tmp[9+i])) == '.') {
				i++;	
			}
			tmp[i]='\0';
			edge = atof(tmp);
			printw("  line  %2d: risetime: %f\n", line, edge);
		}
		else if (tmp == strstr(tmp, "waveform=")) {
			if (isdigit(tmp[i] = tmp[9+i])) {	/* read 1 char only */
				tmp[++i]='\0';
				waveform = atoi(tmp);
			}
			if ((waveform <= 3) && (waveform > 0)) {
				printw("  line  %2d: waveform: %d\n", line, waveform);
			}
			else {
				printw("  line  %2d: waveform: %d invalid. Using default.\n",
						 line, waveform);
				waveform = SINE;
			}
		}
		else if (tmp == strstr(tmp, "constanttone=")) {
			while (isdigit(tmp[i] = tmp[13+i])) {
				i++;    
			}
			tmp[i]='\0';
			k = 0; 
			k = atoi(tmp); 							/* constanttone */
			if ( (k*k) > 1) {
				printw("  line  %2d: constanttone: %s invalid. "
							"Using default %d.\n", line, tmp, constanttone);
			}
			else {
				constanttone = k ;
				printw("  line  %2d: constanttone: %d\n", line, constanttone);
			}
        }
        else if (tmp == strstr(tmp, "ctonefreq=")) {
			while (isdigit(tmp[i] = tmp[10+i])) {
            	i++;    
			}
			tmp[i]='\0';
			k = 0; 
			k = atoi(tmp);							/* ctonefreq */
			if ( (k > 1600) || (k < 100) ) {
				printw("  line  %2d: ctonefreq: %s invalid. "
					"Using default %d.\n", line, tmp, ctonefreq);
			}
			else {
				ctonefreq = k ;
				printw("  line  %2d: ctonefreq: %d\n", line, ctonefreq);
			}
		}
		else if (tmp == strstr(tmp, "f6=")) {
			f6=0;
			if (tmp[3] == '1') {
				f6 = 1;
			}
			printw("  line  %2d: unlimited f6: %s\n", line, (f6 ? "yes":"no"));
        }
		else if (tmp == strstr(tmp, "fixspeed=")) {
			fixspeed=0;
			if (tmp[9] == '1') {
				fixspeed = 1;
			}
			printw("  line  %2d: fixed speed:  %s\n", line, (fixspeed ? "yes":"no"));
        }
		else if (tmp == strstr(tmp, "stoponerror=")) {
			stoponerror = 0;
			if (tmp[12] == '1') {
				stoponerror = 1;
			}
			printw("  line  %2d: stoponerror:  %s\n", line, (stoponerror ? "yes":"no"));
        }
		else if (tmp == strstr(tmp, "unlimitedattempt=")) {
			unlimitedattempt=0;
			if (tmp[17] == '1') {
				unlimitedattempt= 1;
			}
			printw("  line  %2d: unlim. att.:  %s\n", line, (unlimitedattempt ? "yes":"no"));
        }
		else if (tmp == strstr(tmp, "sessionlength=")) {
			while (isdigit((unsigned char)(tmp[i] = tmp[14+i]))) {
				i++;
			}
			tmp[i] = '\0';
			k = atoi(tmp);
			if (k > 0) {
				sessionlength = k;
				printw("  line  %2d: session length: %d\n", line, sessionlength);
			}
			else {
				printw("  line  %2d: session length: >%s< invalid. Using default %d.\n",
						line, tmp, sessionlength);
			}
        }
		else if (tmp == strstr(tmp,"callbase=")) {
			while (isgraph(tmp[i] = tmp[9+i])) {
				i++;
			}
			tmp[i]='\0';
			if (strlen(tmp) > 1) {
				strcpy(cbfilename,tmp);
				printw("  line  %2d: callbase:  >%s<\n", line, cbfilename);
			}
			else {
				printw("  line  %2d: callbase:  >%s< invalid. "
								"Using default >%s<.\n", line, tmp, cbfilename);
			}
		}
		else if (tmp == strstr(tmp,"samplerate=")) {
			while (isdigit(tmp[i] = tmp[11+i])) {
				i++;
			}
			tmp[i]='\0';
			samplerate = atoi(tmp);
			printw("  line  %2d: sample rate: %ld\n", line, samplerate);
		}
	}

	fclose(fh);

	printw("Finished reading qrqrc.\n");
	return 0;
}


static void *morse(void *arg) { 
	char * text = arg;
	int i,j;
	int c, fulldotlen, dotlen, dashlen, charspeed, farnsworth, fwdotlen;
	const char *code;

#if WIN32 /* WinMM simple support by Lukasz Komsta, SP8QED */
	HWAVEOUT		h;
	WAVEFORMATEX	wf;
	WAVEHDR			wh;
	HANDLE			d;

	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 1;
	wf.wBitsPerSample = 16;
	wf.nSamplesPerSec = samplerate * 2;
	wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	wf.cbSize = 0;
	d = CreateEvent(0, FALSE, FALSE, 0);
	if(waveOutOpen(&h, 0, &wf, (DWORD) d, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR);

#else
	/* opening the DSP device */
	dsp_fd = open_dsp(dspdevice);
#endif

#ifdef PA
	if (dsp_fd == NULL) {
		set_sending_complete(1);
		return NULL;
	}
#endif
	/* set bufpos to 0 */

	full_bufpos = 0; 

	/* Some silence; otherwise the call starts right after pressing enter */
	if (tonegen(0, samplerate/4, SILENCE) != 0) {
		goto audio_error;
	}

	/* Farnsworth? */
	if (speed < mincharspeed) {
			charspeed = mincharspeed;
			farnsworth = 1;
			fwdotlen = (int) (samplerate * 6/speed);
	}
	else {
		charspeed = speed;
		farnsworth = 0;
	}

	/* speed is in LpM now, so we have to calculate the dot-length in
	 * milliseconds using the well-known formula  dotlength= 60/(wpm*50) 
	 * and then to samples */

	dotlen = (int) (samplerate * 6/charspeed);
	fulldotlen = dotlen;
	dashlen = 3*dotlen;

	/* edge = length of rise/fall time in ms. ed = in samples */

	ed = (int) (samplerate * (edge/1000.0));

	/* the signal needs "ed" samples to reach the full amplitude and
	 * at the end another "ed" samples to reach zero. The dots and
	 * dashes therefore are becoming longer by "ed" and the pauses
	 * after them are shortened accordingly by "ed" samples */

	for (i = 0; i < (int)strlen(text); i++) {
		c = text[i];
		if (isalpha(c)) {
			code = codetable[c-65];
		}
		else if (isdigit(c)) {
			code = codetable[c-22];
		}
		else if (c == '/') { 
			code = "-..-.";
		}
		else if (c == '+') {
			code = ".-.-.";
		}
        else if (c == ' ') {        /* space */
            code = " ";
        }
        else if (c == '.') {
            code = ".-.-.-";
		}
        else if (c == ',') {
            code = "--..--";
		}
        else if (c == '=') {
            code = "-...-";
        }
		else {						/* not supposed to happen! */
			code = "..--..";
		}
		
		/* code is now available as string with - and . */

		for (j = 0; j < (int)strlen(code); j++) {
			c = code[j];
			if (c == '.') {
				if (tonegen(freq, dotlen + ed, waveform) != 0 ||
					tonegen(0, fulldotlen - ed, SILENCE) != 0) {
					goto audio_error;
				}
			}
			else if (c == '-') {
				if (tonegen(freq, dashlen + ed, waveform) != 0 ||
					tonegen(0, fulldotlen - ed, SILENCE) != 0) {
					goto audio_error;
				}
			}
	            else {  /* space */
				if (tonegen(0, dotlen, SILENCE) != 0) {
					goto audio_error;
				}
	            }
		}
		if (farnsworth) {
			if (tonegen(0, 3*fwdotlen - fulldotlen, SILENCE) != 0) {
				goto audio_error;
			}
		}
		else {
			if (tonegen(0, 2*fulldotlen, SILENCE) != 0) {
				goto audio_error;
			}
		}
	}


#if !defined(PA) && !defined(CA)
	if (add_to_buf(buffer, sizeof(buffer)) != 0) {
		goto audio_error;
	}
#endif

#if WIN32
	wh.lpData = (char*) full_buf;
	wh.dwBufferLength = (DWORD) (full_bufpos - 2);
	wh.dwFlags = 0;
	wh.dwLoops = 0;
	waveOutPrepareHeader(h, &wh, sizeof(wh));
	ResetEvent(d);
	waveOutWrite(h, &wh, sizeof(wh));
	if(WaitForSingleObject(d, INFINITE) != WAIT_OBJECT_0);
	waveOutUnprepareHeader(h, &wh, sizeof(wh));
	waveOutClose(h);
	CloseHandle(d);
#else
	write_audio(dsp_fd, full_buf, (int) full_bufpos);
	close_audio(dsp_fd);
#endif
	set_sending_complete(1);
	return NULL;

audio_error:
#ifdef OSS
	(void) close_audio(dsp_fd);
#endif
	set_sending_complete(1);
	return NULL;
}

static int add_to_buf(const void *data, size_t size)
{
	int *new_buf;
	size_t required;
	size_t new_capacity;

	if (data == NULL || size > (size_t) INT_MAX ||
		full_bufpos > (size_t) INT_MAX - size) {
		return -1;
	}

	required = full_bufpos + size;
	if (required > full_buf_capacity) {
		new_capacity = full_buf_capacity ? full_buf_capacity : 65536;
		while (new_capacity < required) {
			if (new_capacity > (size_t) INT_MAX / 2) {
				new_capacity = required;
				break;
			}
			new_capacity *= 2;
		}

		new_buf = realloc(full_buf, new_capacity);
		if (new_buf == NULL) {
			return -1;
		}
		full_buf = new_buf;
		full_buf_capacity = new_capacity;
	}

	memcpy((unsigned char *) full_buf + full_bufpos, data, size);
	full_bufpos = required;
	return 0;
}

/* tonegen generates a sinus tone of frequency 'freq' and length 'len' (samples)
 * based on 'samplerate', 'edge' (rise/falltime) */

static int tonegen (int freq, int len, int waveform) {
	int x=0;
	int out;
#ifndef PA
	uint32_t stereo_out;
#endif
	double val=0;

	for (x=0; x < len-1; x++) {
		switch (waveform) {
			case SINE:
				val = sin(2*PI*freq*x/samplerate);
				break;
			case SAWTOOTH:
				val=((1.0*freq*x/samplerate)-floor(1.0*freq*x/samplerate))-0.5;
				break;
			case SQUARE:
				val = ceil(sin(2*PI*freq*x/samplerate))-0.5;
				break;
			case SILENCE:
				val = 0;
		}


		if (x < ed) { val *= pow(sin(PI*x/(2.0*ed)),2); }	/* rising edge */

		if (x > (len-ed)) {								/* falling edge */
				val *= pow(sin(2*PI*(x-(len-ed)+ed)/(4*ed)),2); 
		}
		
		out = (int) (val * 32500.0);
#ifndef PA
		stereo_out = ((uint32_t) (uint16_t) out << 16) | (uint16_t) out;
		if (add_to_buf(&stereo_out, sizeof(stereo_out)) != 0) {
			return -1;
		}
#else
		if (add_to_buf(&out, sizeof(out)) != 0) {
			return -1;
		}
#endif
	}
	return 0;
}

/* Save config file
 *
 * Tries to keep the old format (including comments, etc.) and adds
 * config options that were not used yet in the file to the end 
 * */

static int save_config (void) {
	static const char *const confopts[] = {
		"callsign", "callbase", "dspdevice", "initialspeed",
		"mincharspeed", "waveform", "constanttone", "ctonefreq",
		"fixspeed", "unlimitedattempt", "f6", "risetime", "speedstep",
		"speedupstep", "speeddownstep", "sessionlength", "stoponerror"
	};
	FILE *fh = NULL;
	char tmp[PATH_MAX + 80];
	char *config = NULL;
	char *updated;
	char *find;
	char *findend;
	size_t config_len;
	size_t find_offset;
	size_t findend_offset;
	size_t replacement_len;
	size_t updated_len;
	size_t key_len;
	long file_length;
	int i;
	int written;
	int use_crlf;
	int result = -1;

	if ((fh = fopen(rcfilename, "rb")) == NULL) {
		endwin();
		fprintf(stderr, "Unable to open config file '%s'!\n", rcfilename);
		return -1;
	}
	if (fseek(fh, 0, SEEK_END) != 0 || (file_length = ftell(fh)) < 0 ||
			(size_t)file_length == SIZE_MAX) {
		fprintf(stderr, "Unable to determine size of config file '%s'!\n", rcfilename);
		goto cleanup;
	}
	config_len = (size_t)file_length;
	if (fseek(fh, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Unable to rewind config file '%s'!\n", rcfilename);
		goto cleanup;
	}
	config = malloc(config_len + 1);
	if (config == NULL) {
		fprintf(stderr, "Out of memory while reading config file.\n");
		goto cleanup;
	}
	if (config_len != 0 && fread(config, 1, config_len, fh) != config_len) {
		fprintf(stderr, "Unable to read config file '%s'!\n", rcfilename);
		goto cleanup;
	}
	config[config_len] = '\0';
	if (fclose(fh) != 0) {
		fh = NULL;
		fprintf(stderr, "Unable to close config file '%s'!\n", rcfilename);
		goto cleanup;
	}
	fh = NULL;
	use_crlf = strstr(config, "\r\n") != NULL;

	/* Replace only full keys that begin a line, preserving comments and spacing. */
	for (i = 0; i < (int)(sizeof(confopts) / sizeof(confopts[0])); i++) {
		switch (i) {
			case 0: written = snprintf(tmp, sizeof(tmp), "%s=%s ", confopts[i], mycall); break;
			case 1: written = snprintf(tmp, sizeof(tmp), "%s=%s ", confopts[i], cbfilename); break;
			case 2: written = snprintf(tmp, sizeof(tmp), "%s=%s ", confopts[i], dspdevice); break;
			case 3: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], initialspeed); break;
			case 4: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], mincharspeed); break;
			case 5: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], waveform); break;
			case 6: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], constanttone); break;
			case 7: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], ctonefreq); break;
			case 8: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], fixspeed); break;
			case 9: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], unlimitedattempt); break;
			case 10: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], f6); break;
			case 11: written = snprintf(tmp, sizeof(tmp), "%s=%f ", confopts[i], edge); break;
			case 12: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], speedupstep); break;
			case 13: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], speedupstep); break;
			case 14: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], speeddownstep); break;
			case 15: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], sessionlength); break;
			default: written = snprintf(tmp, sizeof(tmp), "%s=%d ", confopts[i], stoponerror); break;
		}
		if (written < 0 || (size_t)written >= sizeof(tmp)) {
			fprintf(stderr, "Unable to format config option '%s'.\n", confopts[i]);
			goto cleanup;
		}
		replacement_len = (size_t)written;
		key_len = strlen(confopts[i]);

		find = config;
		while ((find = strstr(find, confopts[i])) != NULL) {
			if ((find == config || find[-1] == '\n') && find[key_len] == '=') {
				break;
			}
			find++;
		}

		if (find != NULL) {
			findend = find + key_len + 1;
			while (*findend != '\0' && !isspace((unsigned char)*findend)) {
				findend++;
			}
			find_offset = (size_t)(find - config);
			findend_offset = (size_t)(findend - config);
			updated_len = config_len - (findend_offset - find_offset) + replacement_len;
			updated = malloc(updated_len + 1);
			if (updated == NULL) {
				fprintf(stderr, "Out of memory while updating config file.\n");
				goto cleanup;
			}
			memcpy(updated, config, find_offset);
			memcpy(updated + find_offset, tmp, replacement_len);
			memcpy(updated + find_offset + replacement_len, findend,
					config_len - findend_offset + 1);
		}
		else {
			size_t separator_len = config_len != 0 && config[config_len - 1] != '\n' ? 1 : 0;
			size_t newline_len = use_crlf ? 2 : 1;
			if (config_len > SIZE_MAX - separator_len - replacement_len - newline_len) {
				fprintf(stderr, "Config file is too large to update.\n");
				goto cleanup;
			}
			updated_len = config_len + separator_len + replacement_len + newline_len;
			updated = malloc(updated_len + 1);
			if (updated == NULL) {
				fprintf(stderr, "Out of memory while updating config file.\n");
				goto cleanup;
			}
			memcpy(updated, config, config_len);
			find_offset = config_len;
			if (separator_len != 0) {
				updated[find_offset++] = '\n';
			}
			memcpy(updated + find_offset, tmp, replacement_len);
			find_offset += replacement_len;
			if (use_crlf) {
				updated[find_offset++] = '\r';
			}
			updated[find_offset++] = '\n';
			updated[find_offset] = '\0';
		}

		free(config);
		config = updated;
		config_len = updated_len;
	}

	if (write_file_atomic(rcfilename, config, config_len) != 0) {
		endwin();
		fprintf(stderr, "Unable to atomically update config file '%s'!\n", rcfilename);
		goto cleanup;
	}
	result = 0;

cleanup:
	if (fh != NULL) {
		fclose(fh);
	}
	free(config);
	return result;
}
		
static void thread_fail (int j) {
	if (j) {
		endwin();
		perror("Error: Unable to create cwthread!\n");
		exit(EXIT_FAILURE);
	}
}

/* Add timestamps to toplist file if not there yet */
static int check_toplist (void) {
	char first_line[35] = "";
	char *old_data = NULL;
	char *converted = NULL;
	FILE *fh = NULL;
	size_t file_size;
	size_t old_offset;
	size_t new_offset;
	long file_length;
	int result = -1;

	if ((fh = fopen(tlfilename, "rb")) == NULL) {
		endwin();
		perror("Unable to open toplist file 'toplist'!\n");
		return -1;
	}

	if (fgets(first_line, sizeof(first_line), fh) == NULL) {
		if (ferror(fh)) {
			fprintf(stderr, "Unable to read toplist file %s!\n", tlfilename);
			goto cleanup;
		}
		result = 0;
		goto cleanup;
	}
	if (strlen(first_line) != 21) {
		result = 0;
		goto cleanup;
	}

	if (fseek(fh, 0, SEEK_END) != 0 || (file_length = ftell(fh)) < 0) {
		fprintf(stderr, "Unable to determine size of toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	file_size = (size_t)file_length;
	if (file_size % 21 != 0 || file_size / 21 > SIZE_MAX / 32) {
		fprintf(stderr, "Invalid old-format toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	if (fseek(fh, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Unable to rewind toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	old_data = malloc(file_size == 0 ? 1 : file_size);
	converted = malloc(file_size / 21 * 32);
	if (old_data == NULL || converted == NULL) {
		fprintf(stderr, "Out of memory while converting toplist.\n");
		goto cleanup;
	}
	if (file_size != 0 && fread(old_data, 1, file_size, fh) != file_size) {
		fprintf(stderr, "Unable to read toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	if (fclose(fh) != 0) {
		fh = NULL;
		fprintf(stderr, "Unable to close toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	fh = NULL;

	printw("Toplist file in old format. Converting...");
	for (old_offset = 0, new_offset = 0; old_offset < file_size;
			old_offset += 21, new_offset += 32) {
		memcpy(converted + new_offset, old_data + old_offset, 20);
		converted[new_offset + 20] = ' ';
		memcpy(converted + new_offset + 21, "1181234567\n", 11);
	}

	if (write_file_atomic(tlfilename, converted, file_size / 21 * 32) != 0) {
		fprintf(stderr, "Unable to atomically update toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	printw(" done!\n");
	result = 0;

cleanup:
	if (fh != NULL) {
		fclose(fh);
	}
	free(old_data);
	free(converted);
	return result;
}



static int copy_file(const char *source_path, const char *destination_path) {
	char buffer[8192];
	FILE *source = NULL;
	FILE *destination = NULL;
	size_t bytes_read;
	int result = -1;

	source = fopen(source_path, "rb");
	if (source == NULL) {
		return -1;
	}
	destination = fopen(destination_path, "wb");
	if (destination == NULL) {
		goto cleanup;
	}
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) != 0) {
		if (fwrite(buffer, 1, bytes_read, destination) != bytes_read) {
			goto cleanup;
		}
	}
	if (ferror(source) || fclose(destination) != 0) {
		destination = NULL;
		goto cleanup;
	}
	destination = NULL;
	result = 0;

cleanup:
	if (destination != NULL) {
		fclose(destination);
	}
	if (source != NULL) {
		fclose(source);
	}
	return result;
}

/* Write through a same-directory temporary file, then replace the destination.
 * rename() is atomic on POSIX when both paths reside on the same filesystem. */
static int write_file_atomic(const char *path, const void *data, size_t length) {
	char *temporary_path = NULL;
	FILE *temporary_file = NULL;
	size_t path_length;
	int temporary_fd = -1;
	int result = -1;

	path_length = strlen(path);
	if (path_length > SIZE_MAX - 12) {
		return -1;
	}
	temporary_path = malloc(path_length + 12);
	if (temporary_path == NULL) {
		return -1;
	}
	(void)snprintf(temporary_path, path_length + 12, "%s.tmp.XXXXXX", path);
	temporary_fd = mkstemp(temporary_path);
	if (temporary_fd == -1) {
		goto cleanup;
	}
	temporary_file = fdopen(temporary_fd, "wb");
	if (temporary_file == NULL) {
		close(temporary_fd);
		temporary_fd = -1;
		goto cleanup;
	}
	temporary_fd = -1;
	if ((length != 0 && fwrite(data, 1, length, temporary_file) != length) ||
			fflush(temporary_file) != 0) {
		goto cleanup;
	}
#ifndef WIN32
	if (fsync(fileno(temporary_file)) != 0) {
		goto cleanup;
	}
#endif
	if (fclose(temporary_file) != 0) {
		temporary_file = NULL;
		goto cleanup;
	}
	temporary_file = NULL;
#ifdef WIN32
	if (!MoveFileExA(temporary_path, path,
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		goto cleanup;
	}
#else
	if (rename(temporary_path, path) != 0) {
		goto cleanup;
	}
#endif
	result = 0;

cleanup:
	if (temporary_file != NULL) {
		fclose(temporary_file);
	}
	if (result != 0 && temporary_path != NULL) {
		unlink(temporary_path);
	}
	free(temporary_path);
	return result;
}

/* See where our files are. We need 'callbase.qcb', 'qrqrc' and 'toplist'.
 * The can be: 
 * 1) In the current directory -> use them
 * 2) In ~/.qrq/  -> use toplist and qrqrc from there and callbase from
 *    DESTDIR/share/qrq/
 * 3) in DESTDIR/share/qrq/ -> create ~/.qrq/ and copy qrqrc and toplist
 *    there.
 * 4) Nowhere --> Exit.*/
static int find_files (void) {
	
	FILE *fh;
	const char *homedir = NULL;
	char tmp_rcfilename[PATH_MAX] = "";
	char tmp_tlfilename[PATH_MAX] = "";
	char tmp_cbfilename[PATH_MAX] = "";

	printw("\nChecking for necessary files (qrqrc, toplist, callbase)...\n");
	
	if (((fh = fopen("qrqrc", "r")) == NULL) ||
		((fh = fopen("toplist", "r")) == NULL) ||
		((fh = fopen("callbase.qcb", "r")) == NULL)) {
		
		if ((homedir = getenv("HOME")) != NULL) {
    		printw("... not found in current directory. Checking %s/.qrq/...\n", homedir);
    		refresh();
	    	strcat(rcfilename, homedir);
		}
		else {
		    printw("... not found in current directory. Checking ./.qrq/...\n");
    		refresh();
	    	strcat(rcfilename, ".");
		}
				
		strcat(rcfilename, "/.qrq/qrqrc");
	
		/* check if there is ~/.qrq/qrqrc. If it's there, it's safe to assume
		 * that toplist also exists at the same place and callbase exists in
		 * DESTDIR/share/qrq/. */

		if ((fh = fopen(rcfilename, "r")) == NULL ) {
			printw("... not found in %s/.qrq/. Checking %s/share/qrq..."
							"\n", homedir, destdir);
			/* check for the files in DESTDIR/share/qrq/. if exists, copy 
			 * qrqrc and toplist to ~/.qrq/  */

			strcpy(tmp_rcfilename, destdir);
			strcat(tmp_rcfilename, "/share/qrq/qrqrc");
			strcpy(tmp_tlfilename, destdir);
			strcat(tmp_tlfilename, "/share/qrq/toplist");
			strcpy(tmp_cbfilename, destdir);
			strcat(tmp_cbfilename, "/share/qrq/callbase.qcb");

			if (((fh = fopen(tmp_rcfilename, "r")) == NULL) ||
				((fh = fopen(tmp_tlfilename, "r")) == NULL) ||
				 ((fh = fopen(tmp_cbfilename, "r")) == NULL)) {
				printw("Sorry: Couldn't find 'qrqrc', 'toplist' and"
			   			" 'callbase.qcb' anywhere. Exit.\n");
				getch();
				endwin();
				exit(EXIT_FAILURE);
			}
			else {			/* finally found it in DESTDIR/share/qrq/ ! */
				/* abusing rcfilename here for something else temporarily */
				printw("Found files in %s/share/qrq/."
						"\nCreating directory %s/.qrq/ and copy qrqrc and"
						" toplist there.\n", destdir, homedir);
				strcpy(rcfilename, homedir);
				strcat(rcfilename, "/.qrq/");
#ifdef WIN32
				j = mkdir(rcfilename);
#else
				j = mkdir(rcfilename,  0777);
#endif
				if (j && (errno != EEXIST)) {
					printw("Failed to create %s! Exit.\n", rcfilename);
					getch();
					endwin();
					exit(EXIT_FAILURE);
				}

				if (snprintf(tlfilename, sizeof(tlfilename), "%s/.qrq/toplist", homedir) >=
						(int)sizeof(tlfilename) || copy_file(tmp_tlfilename, tlfilename) != 0) {
					printw("Failed to copy toplist file.\n");
					getch();
					endwin();
					exit(EXIT_FAILURE);
				}
				if (snprintf(rcfilename, sizeof(rcfilename), "%s/.qrq/qrqrc", homedir) >=
						(int)sizeof(rcfilename) || copy_file(tmp_rcfilename, rcfilename) != 0) {
					printw("Failed to copy qrqrc file.\n");
					getch();
					endwin();
					exit(EXIT_FAILURE);
				}
				printw("Files copied. You might want to edit "
						"qrqrc according to your needs.\n");
				strcpy(cbfilename, tmp_cbfilename);
                strcpy(sumfilepath, homedir);
                strcat(sumfilepath, "/.qrq/Summary");
			} /* found in DESTDIR/share/qrq/ */
		}
		else {
			printw("... found files in %s/.qrq/.\n", homedir);
			strcpy(tlfilename, homedir);
			strcat(tlfilename, "/.qrq/toplist");
			strcpy(cbfilename, destdir);
			strcat(cbfilename, "/share/qrq/callbase.qcb");
            strcpy(sumfilepath, homedir);
            strcat(sumfilepath, "/.qrq/Summary");
		}
	}
	else {
		printw("... found in current directory.\n");
		strcpy(rcfilename, "qrqrc");
		strcpy(tlfilename, "toplist");
		strcpy(cbfilename, "callbase.qcb");
        strcpy(sumfilepath, "Summary");
	}
#ifdef WIN32
    mkdir(sumfilepath);
#else
    mkdir(sumfilepath, 0777);
#endif
	refresh();
	fclose(fh);
	return 0;
}


static int statistics (void) {
		char line[80]="";

		int time = 0;
		int score = 0;
		int count= 0;

		FILE *fh;
		FILE *fh2;
#ifndef WIN32
		int plot_pipe[2];
		pid_t gnuplot_pid;
#endif
		
		if ((fh = fopen(tlfilename, "r")) == NULL) {
				fprintf(stderr, "Unable to open toplist.");
				return -1;
		}

#ifdef WIN32
		/* _popen supplies gnuplot with the script on standard input. */
		fh2 = _popen("start /B gnuplot -p", "w");
#else
		if (pipe(plot_pipe) != 0) {
				fclose(fh);
				fprintf(stderr, "Unable to create gnuplot pipe.\n");
				return -1;
		}
		gnuplot_pid = fork();
		if (gnuplot_pid == 0) {
				int null_fd = open("/dev/null", O_WRONLY);
				close(plot_pipe[1]);
				if (dup2(plot_pipe[0], STDIN_FILENO) == -1 ||
						(null_fd != -1 && dup2(null_fd, STDERR_FILENO) == -1)) {
					_exit(EXIT_FAILURE);
				}
				close(plot_pipe[0]);
				if (null_fd != -1) {
					close(null_fd);
				}
				execlp("gnuplot", "gnuplot", "-p", (char *)NULL);
				_exit(EXIT_FAILURE);
		}
		if (gnuplot_pid < 0) {
				close(plot_pipe[0]);
				close(plot_pipe[1]);
				fclose(fh);
				fprintf(stderr, "Unable to start gnuplot.\n");
				return -1;
		}
		close(plot_pipe[0]);
		fh2 = fdopen(plot_pipe[1], "w");
#endif
		if (fh2 == NULL) {
#ifndef WIN32
			close(plot_pipe[1]);
#endif
				fclose(fh);
				fprintf(stderr, "Unable to open gnuplot input.\n");
				return -1;
		}

		fprintf(fh2, "set yrange [0:]\nset xlabel \"Date/Time\"\n"
					"set title \"QRQ scores for %s. Press 'q' to "
					"close this window.\"\n"
					"set ylabel \"Score\"\nset xdata time\nset "
					" timefmt \"%%s\"\n "
					"plot \"-\" using 1:2 title \"\"\n", mycall);

		while ((feof(fh) == 0) && (fgets(line, 80, fh) != NULL)) {
				if ((strstr(line, mycall) != NULL)) {
					count++;
					sscanf(line, "%*s %d %*d %d", &score, &time);
					fprintf(fh2, "%d %d\n", time, score);
				}
		}

		if (!count) {
			fprintf(fh2, "0 0\n");
		}
		
		fprintf(fh2, "end\npause 10000");

		if (fclose(fh) != 0) {
			fprintf(stderr, "Unable to close toplist.\n");
		}
#ifdef WIN32
		if (_pclose(fh2) == -1) {
			fprintf(stderr, "Unable to close gnuplot input.\n");
			return -1;
		}
#else
		if (fclose(fh2) != 0) {
			fprintf(stderr, "Unable to close gnuplot input.\n");
			return -1;
		}
#endif
	return 0;
}


static void free_calls(void) {
	size_t i;

	for (i = 0; i < calls_allocated; i++) {
		free(calls[i]);
	}
	free(calls);
	calls = NULL;
	calls_allocated = 0;
}

int read_callbase (void) {
	FILE *fh;
	int c,i;
	char tmp[CALL_MAX + 2] = "";
	int nr=0;

	if ((fh = fopen(cbfilename, "r")) == NULL) {
		endwin();
		fprintf(stderr, "Error: Couldn't read callsign database ('%s')!\n",
						cbfilename);
		exit(EXIT_FAILURE);
	}

	/* count the lines/calls and lengths */
	call_maxlen = 0;
	i=0;
	while ((c = getc(fh)) != EOF) {
		i++;
		if (c == '\n') {
			nr++;
			call_maxlen = (i > call_maxlen) ? i : call_maxlen;
			i = 0;
		}
	}
	call_maxlen--; /* remove \n */

	if (!nr) {
		endwin();
		printf("\nError: Callsign database empty, no calls read. Exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (call_maxlen > CALL_MAX) {
		endwin();
		printf("\nError: Callsign database contains a line with %d letters, which is longer than CALL_MAX (%d) . Exiting.\n", call_maxlen, CALL_MAX);
		exit(EXIT_FAILURE);
	}

	/* Allocate a zero-initialized table so cleanup is safe after a partial
	 * allocation failure. */
	free_calls();
	calls = calloc((size_t) nr, sizeof(*calls));
	calls_allocated = (size_t) nr;
	if (calls == NULL) {
		fprintf(stderr, "Error: Couldn't allocate %d bytes!\n", 
						(int) sizeof(*calls)*nr);
		exit(EXIT_FAILURE);
	}
	
	/* Allocate each element of the array with size maxlen */
	for (c=0; c < nr; c++) {
		if ((calls[c] = (char *) malloc ((call_maxlen + 2) * sizeof(char))) == NULL) {
			fprintf(stderr, "Error: Couldn't allocate %d bytes!\n", call_maxlen + 2);
			free_calls();
			exit(EXIT_FAILURE);
		}
	}

	rewind(fh);
	
	nr=0;
	while (fgets(tmp,call_maxlen+2,fh) != NULL) {
		for (i = 0; i < (int)strlen(tmp); i++) {
				tmp[i] = toupper(tmp[i]);
		}
		tmp[i-1]='\0';				/* remove newline */
		if (tmp[i-2] == '\r') {		/* also for DOS files */
			tmp[i-2] = '\0';
		}
		strcpy(calls[nr],tmp);
		nr++;
		if (nr == c) 			/* may happen if call file corrupted */
				break;
	}
	fclose(fh);


	return nr;

}

void find_callbases (void) {
	DIR *dir;
	struct dirent *dp;
	char tmp[PATH_MAX];
	char path[3][PATH_MAX];
	int i=0,j=0,k=0;

#ifndef WIN32
		strcpy(path[0], getenv("PWD"));
		strcat(path[0], "/");
		strcpy(path[1], getenv("HOME"));
		strcat(path[1], "/.qrq/");
		strcpy(path[2], destdir);
		strcat(path[2], "/share/qrq/");
#else
		strcpy(path[0], "./");
		strcpy(path[1], getenv("APPDATA"));
		strcat(path[1], "/qrq/");
		strcpy(path[2], "c:\\");
#endif

	for (i=0; i < 100; i++) {
		strcpy(cblist[i], "");
	}

	/* foreach paths...  */
	for (k = 0; k < 3; k++) {

		if (!(dir = opendir(path[k]))) {
			continue;
		}
	
		while ((dp = readdir(dir))) {
			strcpy(tmp, dp->d_name);
			i = strlen(tmp);
			/* find *.qcb files ...  */
			if (i>4 && tmp[i-1] == 'b' && tmp[i-2] == 'c' && tmp[i-3] == 'q') {
				strcpy(cblist[j], path[k]);
				strcat(cblist[j], tmp);
				j++;
			}
		}
	} /* for paths */
}



void select_callbase (void) {
	int i = 0, j = 0, k = 0;
	int c = 0;		/* cursor position   */
	int p = 0;		/* page a 10 entries */
	char* cblist_ptr;


	curs_set(FALSE);

	/* count files */
	while (strcmp(cblist[i], "")) i++;

	if (!i) {
		mvwprintw(conf_w,10,4, "No qcb-files found!");
		wrefresh(conf_w);
#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
		return;
	}

	/* loop for key unput */
	while (1) {

	/* cls */
	for (j = 5; j < 16; j++) {
			mvwprintw(conf_w,j,2, "                                         ");
	}

	/* display 10 files, highlight cursor position */
	for (j = p*10; j < (p+1)*10; j++) {
		if (j <= i) {
				cblist_ptr = cblist[j];
				mvwprintw(conf_w,5+(j - p*10 ),2, "  %s       ", cblist_ptr);
		}
		if (c == j) {						/* cursor */
			mvwprintw(conf_w,5+(j - p*10),2, ">");
		}
	}
	
	wrefresh(conf_w);

	k = getch();

	switch ((int) k) {
		case KEY_UP:
			c = (c > 0) ? (c-1) : c;
			if (!((c+1) % 10)) {	/* scroll down */
				p = (p > 0) ? (p-1) : p;
			}
			break;
		case KEY_DOWN:
			c = (c < i-1) ? (c+1) : c;
			if (c && !(c % 10)) {	/* scroll down */
				p++;
			}
			break;
		case '\n':
			strcpy(cbfilename, cblist[c]);
			nrofcalls = read_callbase();
			return;	
			break;
	}

	wrefresh(conf_w);

	} /* while 1 */

	curs_set(TRUE);

}

int validchar (int c) {
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0'  && c <= '9') || c == '/' || c == ' ' || c == '-' || c == '.' || c == ',' || c == '=' || c == '?');
}

void help (void) {
		printf("qrq v%s  (c) 2006-2025 Fabian Kurz, DJ5CW. "
					"http://fkurz.net/ham/qrq.html\n", VERSION);
		printf("High speed morse telegraphy trainer, similar to"
					" RUFZ.\n\n");
		printf("This is free software, and you are welcome to" 
						" redistribute it\n");
		printf("under certain conditions (see COPYING).\n\n");
		printf("Start 'qrq' without any command line arguments for normal"
					" operation.\n\n");
#ifdef BUILD_INFO
        printf("Build info for this executable:\n%s\n", BUILD_INFO);
#endif
		exit(0);
}


/* vim: noai:ts=4:sw=4 
*/
