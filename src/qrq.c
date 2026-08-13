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
#ifdef WIN32
#include <pdcurses.h>
#else
#include <ncurses.h>
#endif
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
#ifndef WIN32
#include <signal.h>
#include <sys/time.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif

#ifdef WIN_THREADS
#define MORSE_THREAD_RETURN unsigned __stdcall
#define MORSE_THREAD_RESULT 0U
#else
#define MORSE_THREAD_RETURN void *
#define MORSE_THREAD_RESULT NULL
#endif

#define PI 3.14159265358979323846

#define SILENCE 0		/* Waveforms for the tone generator */
#define SINE 1
#define SAWTOOTH 2
#define SQUARE 3

#define CAPITALS_ON   1
#define CAPITALS_OFF  0

#define CALL_MAX    28    /* maximum allowed length of a call/word. limit to 28 so we can fit word + correction into the window */

#ifndef PREFIX
#	define PREFIX "/usr"
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
typedef int AUDIO_HANDLE;
#endif

#ifdef PA
#include "pulseaudio.h"
typedef void *AUDIO_HANDLE;
#endif

#include "score.h"
#include "callbase.h"
#include "practice.h"
#include "history.h"
#include "confusion.h"
#include "item_history.h"
#include "audio_effects.h"
#include "config.h"

/* callsign array will be dynamically allocated */
static char **calls = NULL;
static size_t calls_allocated = 0;
static unsigned char *call_used = NULL;
static unsigned char *call_mistakes = NULL;
static unsigned char *call_spaced_due = NULL;
static struct qrq_review_queue review_queue = {0};
static struct qrq_callbase loaded_callbase = {0};

static const char *codetable[] = {
".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",".---",
"-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",
".--","-..-","-.--","--..","-----",".----","..---","...--","....-",".....",
"-....", "--...","---..","----."};

/* List of available callbase files. Probably no need to do dynamic memory allocation for that list.... */

static char cblist[100][PATH_MAX];
static size_t cblist_count = 0;

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
static size_t displayed_errors=0;		/* errors placed in the current display */
static int p=0;							/* position of cursor, relative to x */
static int status=1;					/* 1= attempt, 2=config */
static int mode=1;						/* 0 = overwrite, 1 = insert */
static int constanttone=0;              /* if 1 don't change the pitch */
static int ctonefreq=800;               /* if constanttone=1 use this freq */
static int volume=100;                  /* output gain as a percentage */
static int qrnlevel=0;                  /* background noise as a percentage */
static uint32_t qrn_state=0x6d2b79f5U;  /* separate from practice rand() state */
static int qsblevel=0;                  /* signal fade depth as a percentage */
static double qsb_phase=0.0;
static int qrmlevel=0;                  /* co-channel interfering CW percentage */
static struct qrq_qrm_state qrm_state;
static size_t qrm_dot_samples=1;
static int minpitch=500;
static int maxpitch=900;
static int f6=0;						/* f6 = 1: allow unlimited repeats */
static int fixspeed=0;					/* keep speed fixed, regardless of err*/
static int unlimitedattempt=0;			/* attempt with all calls  of the DB */
static int sessionlength=50;				/* calls per standard practice session */
static int mincalllength=1;
static int maxcalllength=CALL_MAX;
static char callprefixes[128]="";
static int digitmode=0;
static int portablemode=0;
static int portablevariants=0;
static char allowedchars[CALL_MAX + 1]="";
static int adaptiveselection=0;
static int reviewmisses=0;
static int focusconfusions=0;
static int focusconfusions_active=0;
static int spacedrepetition=0;
static size_t spaced_due_count=0;
static int answerbatch=1;
static int serialdigits=0;
static int accuracytarget=0;
static int goalspeed=0;
static int goalduration=0;
static unsigned int sessionseed=0;
static int attemptvalid=1;				/* 1 = not using any "cheats" */
static size_t nrofcalls=0;
static int toplist_own=0;               /* show only own call on toplist */
static int call_maxlen = 0;				/* maximum length of a callsign/word from current database */
static int parameter_page = 0;
#ifndef WIN32
static volatile sig_atomic_t resize_pending = 0;
#endif

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
static MORSE_THREAD_RETURN morse(void *arg);
static int add_to_buf(const void *data, size_t size);
static double qrn_sample(void);
static int readline(WINDOW *win, int y, int x, char *line, int capitals,
		size_t capacity, int display_width, int path_mode);
static void draw_input_line(WINDOW *win, int y, int x, const char *line,
		int display_width);
static WINDOW *create_window(int height, int width, int y, int x);
static void start_morse_thread(const char *text);
static void wait_morse_thread(void);
static int check_toplist (void);
static int find_files (void);
static int statistics (void);
static size_t read_callbase(void);
static void find_callbases(void);
static void select_callbase (void);
static void help(void);
static void print_version(void);
static void callbase_dialog(void);
static void parameter_dialog(void);
static int clear_parameter_display(void);
static void update_parameter_dialog(void);
static void start_summary_file(void);
static void close_summary_file(void);
static int append_summary(const char *format, ...);
#ifndef WIN32
static void note_terminal_resize(int signal_number);
static void apply_terminal_resize(void);
#endif
static int validchar(int c);
static void free_calls(void);
static void apply_confusion_focus(void);
static int copy_file(const char *source_path, const char *destination_path);
static int write_file_atomic(const char *path, const void *data, size_t length);
#ifdef OSX_BUNDLE
static int set_bundle_resource_directory(const char *program_path);
#endif

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

static uint64_t timestamp_milliseconds(void) {
#ifdef WIN32
	return (uint64_t)GetTickCount64();
#else
	struct timeval timestamp;

	if (gettimeofday(&timestamp, NULL) != 0 || timestamp.tv_sec < 0 ||
			timestamp.tv_usec < 0 || timestamp.tv_usec >= 1000000L ||
			(uint64_t)timestamp.tv_sec > UINT64_MAX / 1000U) {
		return 0;
	}
	return (uint64_t)timestamp.tv_sec * 1000U +
			(uint64_t)timestamp.tv_usec / 1000U;
#endif
}

static uint64_t elapsed_milliseconds(uint64_t started, uint64_t finished) {
	return finished >= started ? finished - started : 0;
}

static uint64_t add_elapsed_milliseconds(uint64_t total, uint64_t started,
		uint64_t finished) {
	uint64_t elapsed = elapsed_milliseconds(started, finished);

	return total > UINT64_MAX - elapsed ? UINT64_MAX : total + elapsed;
}


#ifdef WIN_THREADS
static HANDLE cwthread = NULL;
#else
static pthread_t cwthread;		/* thread for CW output, to enable
								   keyboard reading at the same time */
#endif
static int cwthread_active = 0;

static WINDOW *create_window(int height, int width, int y, int x) {
	WINDOW *window = newwin(height, width, y, x);

	if (window == NULL) {
		endwin();
		fprintf(stderr, "Unable to create the %dx%d terminal layout.\n",
				COLS, LINES);
		exit(EXIT_FAILURE);
	}
	return window;
}

static void start_morse_thread(const char *text) {
	set_sending_complete(0);
#ifdef WIN_THREADS
	cwthread = (HANDLE)_beginthreadex(NULL, 0, morse, (void *)text, 0, NULL);
	if (cwthread == NULL) {
		int saved_errno = errno;
		set_sending_complete(1);
		endwin();
		fprintf(stderr, "Unable to create CW thread: %s\n", strerror(saved_errno));
		exit(EXIT_FAILURE);
	}
#else
	{
		int result = pthread_create(&cwthread, NULL, morse, (void *)text);
		if (result != 0) {
			set_sending_complete(1);
			endwin();
			fprintf(stderr, "Unable to create CW thread: %s\n", strerror(result));
			exit(EXIT_FAILURE);
		}
	}
#endif
	cwthread_active = 1;
}

static void wait_morse_thread(void) {
	if (!cwthread_active) {
		return;
	}
#ifdef WIN_THREADS
	{
		DWORD wait_result = WaitForSingleObject(cwthread, INFINITE);
		DWORD wait_error = wait_result == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
		BOOL close_result = CloseHandle(cwthread);

		cwthread = NULL;
		cwthread_active = 0;
		if (wait_result != WAIT_OBJECT_0 || !close_result) {
			endwin();
			fprintf(stderr, "Unable to wait for CW thread (wait=%lu, error=%lu).\n",
					(unsigned long)wait_result, (unsigned long)wait_error);
			exit(EXIT_FAILURE);
		}
	}
#else
	{
		int result = pthread_join(cwthread, NULL);
		cwthread_active = 0;
		if (result != 0) {
			endwin();
			fprintf(stderr, "Unable to join CW thread: %s\n", strerror(result));
			exit(EXIT_FAILURE);
		}
	}
#endif
}

char rcfilename[PATH_MAX]="";			/* filename and path to qrqrc */
char tlfilename[PATH_MAX]="";			/* filename and path to toplist */
char historyfilename[PATH_MAX]="";
char confusionfilename[PATH_MAX]="";
char itemhistoryfilename[PATH_MAX]="";
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
  /* if built as osx bundle set the resource prefix to its Resources dir */
#ifdef OSX_BUNDLE
	if (set_bundle_resource_directory(argv[0]) != 0) {
		fprintf(stderr, "Unable to locate the application bundle resources.\n");
		return EXIT_FAILURE;
	}
#else
	int prefix_length;

	(void)argv;
	prefix_length = snprintf(destdir, sizeof(destdir), "%s", PREFIX);
	if (prefix_length < 0 || (size_t)prefix_length >= sizeof(destdir)) {
		fprintf(stderr, "The configured resource prefix is too long.\n");
		return EXIT_FAILURE;
	}
#endif

	char abort = 0;
	char tmp[CALL_MAX + 1]="";
	char input[CALL_MAX + 1]="";
	size_t selected_index;
	int completedcalls = 0;
	int attemptaccuracy;
	int sessioneligible;
	int sustainedgoal;
	int sustainedgoalmet;
	int sustainedgoalspeedviolated;
	int toplist_score;
	int made_error;
	int i=0,j=0,k=0;						/* counter etc. */
	int attempt_limit;
	int batch_indices[QRQ_PRACTICE_MAX_ANSWER_BATCH];
	int batch_frequencies[QRQ_PRACTICE_MAX_ANSWER_BATCH];
	int batch_start;
	int session_answerbatch;
	int sessiongoalduration;
	int sessiongoalspeed;
	int stop_attempt;
	size_t batch_count;
	size_t batch_position;
	size_t batch_target;
	char previouscall[CALL_MAX + 1]="";
	int previousfreq = 0;
	int f6pressed=0;
	uint64_t response_started_ms;
	uint64_t replay_started_ms;
	uint64_t replay_duration_ms;
	uint64_t response_finished_ms;
	uint64_t response_ms;
	uint64_t sustainedgoalelapsed_ms;
	uint64_t sustainedgoalstart_ms;

	if (argc == 2 && strcmp(argv[1], "--version") == 0) {
		print_version();
		return EXIT_SUCCESS;
	}
	if (argc > 1) {
		help();
	}
	
	(void) initscr();
	if (LINES < 24 || COLS < 80) {
		endwin();
		fprintf(stderr, "QRQ requires a terminal of at least 80 columns by 24 rows "
				"(current: %d by %d).\n", COLS, LINES);
		return EXIT_FAILURE;
	}
	cbreak();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	scrollok(stdscr, FALSE);
#ifndef WIN32
	signal(SIGWINCH, note_terminal_resize);
#endif
	
	printw("qrq v%s - Copyright (C) 2006-2021 Fabian Kurz, DJ5CW\n", VERSION);
	printw("This is free software, and you are welcome to redistribute it\n");
	printw("under certain conditions (see COPYING).\n");

	refresh();

	/* search for 'toplist', 'qrqrc' and callbase.qcb and put their locations
	 * into tlfilename, rcfilename, cbfilename */
	find_files();
	if (snprintf(historyfilename, sizeof(historyfilename), "%s.history.csv", tlfilename) >=
			(int)sizeof(historyfilename)) {
		endwin();
		fprintf(stderr, "History filename is too long.\n");
		return EXIT_FAILURE;
	}
	if (snprintf(confusionfilename, sizeof(confusionfilename), "%s.confusions.csv",
			tlfilename) >= (int)sizeof(confusionfilename)) {
		endwin();
		fprintf(stderr, "Confusion-history filename is too long.\n");
		return EXIT_FAILURE;
	}
	if (snprintf(itemhistoryfilename, sizeof(itemhistoryfilename), "%s.items.csv",
			tlfilename) >= (int)sizeof(itemhistoryfilename)) {
		endwin();
		fprintf(stderr, "Item-history filename is too long.\n");
		return EXIT_FAILURE;
	}

	/* check if the toplist is in the suitable format. as of 0.0.7, each line
	 * is 31 characters long, with the added time stamp */
	if (check_toplist() != 0) {
		endwin();
		return EXIT_FAILURE;
	}

	/* buffer for audio */
	for (long_i=0;long_i<88200;long_i++) {
		buffer[long_i]=0;
	}
	
	/* Practice selection and simulated receiver noise use independent streams. */
	qrn_state ^= (uint32_t)time(NULL);
	qrq_qrm_init(&qrm_state, qrn_state ^ 0xa5d4e3f1U);
	srand((unsigned)time(NULL));

	/****** Reading configuration file ******/
	printw("\nReading configuration file qrqrc \n");
	read_config();

	attemptvalid = 1;
	if (f6 || fixspeed || unlimitedattempt || sessionlength != 50 || adaptiveselection || reviewmisses || focusconfusions || spacedrepetition || answerbatch != 1 || serialdigits != 0 || portablevariants || sessionseed != 0 || qrq_practice_sustained_goal_active(goalspeed, goalduration)) {
		attemptvalid = 0;	
	}

	/****** Reading callsign database ******/
	printw("\nReading callsign database... ");
	nrofcalls = read_callbase();

	printw("done. %zu calls read.\n\n", nrofcalls);
	printw("Press any key to continue...");

	refresh();
	getch();

	erase();
	refresh();

	top_w = create_window(4, 60, 0, 0);
	mid_w = create_window(17, 60, 4, 0);
	conf_w = create_window(17, 60, 4, 0);
	bot_w = create_window(3, 60, 21, 0);
	inf_w = create_window(3, 60, 21, 0);
	right_w = create_window(24, 20, 0, 60);

	werase(top_w);
	werase(mid_w);
	werase(conf_w);
	werase(bot_w);
	werase(inf_w);
	werase(right_w);

	keypad(bot_w, TRUE);
	keypad(mid_w, TRUE);
	keypad(conf_w, TRUE);

	start_morse_thread("QRQ");

/* very outter loop */
while (1) {	
#ifndef WIN32
	apply_terminal_resize();
#endif

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
	if (qrq_practice_sustained_goal_active(goalspeed, goalduration)) {
		mvwprintw(mid_w,2,2, "Copy for %d sec, staying at %d CpM or higher.",
				goalduration, goalspeed);
		mvwaddstr(mid_w,3,2, "A drop below target fails this training-only goal.");
	}
	else if (unlimitedattempt) {
		mvwaddstr(mid_w,2,2, "After entering your callsign, all random callsigns");
		mvwaddstr(mid_w,3,2, "from a database will be sent. After each callsign,");
	}
	else {
		mvwprintw(mid_w,2,2, "After entering your callsign, %d random callsigns",
				sessionlength);
		mvwaddstr(mid_w,3,2, "from a database will be sent. After each callsign,");
	}
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
	mvwaddstr(mid_w,14,2, "Practice statistics and copy differences with F7.");
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
	maxspeed = errornr = score = completedcalls = 0;
	displayed_errors = 0;
	speed = initialspeed;
	
	/* prompt for own callsign */
	i = readline(bot_w, 1, 30, mycall, CAPITALS_ON, sizeof(mycall), 8, 0);

	/* F5 -> Configure sound */
	if (i == 5) {
		parameter_dialog();
		break;
	} 
	/* F6 -> play test CW */
	else if (i == 6) {
		freq = constanttone ? ctonefreq : 800;
		wait_morse_thread();
		start_morse_thread("VVVTEST");
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
	if (sessionseed != 0) {
		srand(sessionseed);
	}
	session_answerbatch = answerbatch;
	sessiongoalspeed = goalspeed;
	sessiongoalduration = goalduration;
	sustainedgoal = qrq_practice_sustained_goal_active(sessiongoalspeed,
			sessiongoalduration);
	sustainedgoalstart_ms = timestamp_milliseconds();
	sustainedgoalspeedviolated = sustainedgoal && speed < sessiongoalspeed;
	sustainedgoalelapsed_ms = 0;
	sustainedgoalmet = 0;

	/****** send a configured number of calls, ask for input, score ******/
    start_summary_file();
	if (nrofcalls > (size_t)INT_MAX) {
		attempt_limit = INT_MAX;
	}
	else if (sustainedgoal || unlimitedattempt || sessionlength > (int)nrofcalls) {
		attempt_limit = (int)nrofcalls;
	}
	else {
		attempt_limit = sessionlength;
	}
	qrq_review_queue_clear(&review_queue);

	for (callnr = 1, stop_attempt = 0;
			callnr <= attempt_limit && !stop_attempt && (!sustainedgoal ||
			!qrq_practice_sustained_goal_expired(sessiongoalspeed,
					sessiongoalduration,
					elapsed_milliseconds(sustainedgoalstart_ms,
					timestamp_milliseconds())));) {
		batch_start = callnr;
		batch_target = qrq_practice_answer_batch_size(
				(size_t)(attempt_limit - callnr + 1), session_answerbatch);
		batch_count = 0;

		/* Send one or more items before accepting any entry. Reserving each
		 * selected item prevents duplicate calls within a delayed batch. */
		while (batch_count < batch_target && callnr <= attempt_limit &&
				(!sustainedgoal || !qrq_practice_sustained_goal_expired(sessiongoalspeed,
					sessiongoalduration, elapsed_milliseconds(sustainedgoalstart_ms,
					timestamp_milliseconds())))) {
			wait_morse_thread();
			if (reviewmisses && callnr % QRQ_REVIEW_INTERVAL == 0 &&
					qrq_review_queue_take(&review_queue, &selected_index)) {
				if (selected_index > (size_t)INT_MAX) {
					fprintf(stderr, "Review queue contains an invalid callbase index.\n");
					stop_attempt = 1;
					break;
				}
				i = (int)selected_index; /* Review entries may already be marked used. */
			} else {
				selected_index = qrq_practice_choose_scheduled(nrofcalls, call_used,
						call_mistakes, adaptiveselection, call_spaced_due,
						spacedrepetition, (uint32_t)rand());
				if (selected_index == QRQ_PRACTICE_NO_ITEM ||
						selected_index > (size_t)INT_MAX) {
					i = -1;
				} else {
					i = (int)selected_index;
				}
			}
			if (i < 0) {
				fprintf(stderr, "No unused callbase entries remain.\n");
				stop_attempt = 1;
				break;
			}
			if (constanttone == 0) {
				freq = minpitch + (int)((unsigned int)rand() %
						(unsigned int)(maxpitch - minpitch + 1));
			} else {
				freq = ctonefreq;
			}
			batch_indices[batch_count] = i;
			batch_frequencies[batch_count] = freq;
			call_used[i] = 1;
			mvwaddstr(bot_w, 1, 1, "                                                          ");
			if (session_answerbatch > 1) {
				mvwprintw(bot_w, 1, 1, "Copying batch: call %d of %d", callnr,
						attempt_limit);
			} else {
				mvwprintw(bot_w, 1, 1, "%3d/%s", callnr,
						unlimitedattempt ? "-" : "");
				if (!unlimitedattempt) {
					wprintw(bot_w, "%d", attempt_limit);
				}
			}
			wrefresh(bot_w);
			if (session_answerbatch == 1) {
				response_started_ms = timestamp_milliseconds();
			}
			start_morse_thread(calls[i]);
			batch_count++;
			callnr++;
		}
		if (batch_count == 0) {
			break;
		}
		if (session_answerbatch > 1) {
			wait_morse_thread();
		}

		for (batch_position = 0; batch_position < batch_count; ++batch_position) {
			i = batch_indices[batch_position];
			freq = batch_frequencies[batch_position];
			callnr = batch_start + (int)batch_position;
			mvwaddstr(bot_w, 1, 1, "                                                          ");
			mvwprintw(bot_w, 1, 1, "%3d/%s", callnr, unlimitedattempt ? "-" : "");
			if (!unlimitedattempt) {
				wprintw(bot_w, "%d", attempt_limit);
			}
			if (session_answerbatch > 1) {
				mvwaddstr(bot_w, 1, 30, "F6/F7 unavailable");
				response_started_ms = timestamp_milliseconds();
			}
			wrefresh(bot_w);
			tmp[0] = '\0';
			replay_started_ms = 0;
			replay_duration_ms = 0;
			f6pressed = 0;

			while (!abort && (j = readline(bot_w, 1, 8, input, CAPITALS_ON,
					sizeof(input), CALL_MAX, 0)) > 4) {
				if (session_answerbatch > 1 && (j == 6 || j == 7)) {
					mvwaddstr(bot_w, 1, 30, "F6/F7 unavailable");
					wrefresh(bot_w);
					continue;
				}
				switch (j) {
					case 6:
						if (f6pressed && f6 == 0) {
							continue;
						}
						f6pressed = 1;
						wait_morse_thread();
						if (replay_started_ms != 0) {
							replay_duration_ms = add_elapsed_milliseconds(
									replay_duration_ms, replay_started_ms,
									timestamp_milliseconds());
						}
						start_morse_thread(calls[i]);
						replay_started_ms = timestamp_milliseconds();
						break;
					case 7:
						if (callnr > 1) {
							k = freq;
							freq = previousfreq;
							wait_morse_thread();
							if (replay_started_ms != 0) {
								replay_duration_ms = add_elapsed_milliseconds(
										replay_duration_ms, replay_started_ms,
										timestamp_milliseconds());
								replay_started_ms = 0;
							}
							replay_started_ms = timestamp_milliseconds();
							start_morse_thread(previouscall);
							wait_morse_thread();
							replay_duration_ms = add_elapsed_milliseconds(
									replay_duration_ms, replay_started_ms,
									timestamp_milliseconds());
							replay_started_ms = 0;
							freq = k;
						}
						break;
					case 10:
						abort = 1;
						break;
				}
			}
			if (abort) {
				abort = 0;
				input[0] = '\0';
				stop_attempt = 1;
				break;
			}
			response_finished_ms = timestamp_milliseconds();
			if (replay_started_ms != 0) {
				wait_morse_thread();
				response_finished_ms = timestamp_milliseconds();
				replay_duration_ms = add_elapsed_milliseconds(replay_duration_ms,
						replay_started_ms, response_finished_ms);
			}
			response_ms = elapsed_milliseconds(response_started_ms, response_finished_ms);
			response_ms = response_ms >= replay_duration_ms ?
					response_ms - replay_duration_ms : 0;

			score = qrq_score_accumulate(score,
					calc_score(calls[i], input, speed, tmp, f6pressed));
			if (sustainedgoal && speed < sessiongoalspeed) {
				sustainedgoalspeedviolated = 1;
			}
			completedcalls++;
			update_score();
			made_error = strcmp(tmp, "-") != 0;
			if (qrq_item_history_append(itemhistoryfilename, mycall, calls[i],
					!made_error, response_ms) != 0) {
				fprintf(stderr, "Unable to record item history in %s.\n",
						itemhistoryfilename);
			}
			if (qrq_practice_record_result(nrofcalls, call_used, call_mistakes,
					(size_t)i, !made_error, adaptiveselection) != 0) {
				fprintf(stderr, "Unable to record practice result.\n");
				stop_attempt = 1;
				break;
			}
			if (made_error) {
				if (qrq_confusion_append(confusionfilename, mycall, calls[i], input) != 0) {
					fprintf(stderr, "Unable to record copy differences in %s.\n",
							confusionfilename);
				}
				if (reviewmisses && qrq_review_queue_push(&review_queue, (size_t)i) != 0) {
					fprintf(stderr, "Unable to queue missed call for review.\n");
				}
				show_error(calls[i], tmp);
				if (stoponerror) {
					getch();
				}
			}
			input[0] = '\0';
			strncpy(previouscall, calls[i], CALL_MAX);
			previouscall[CALL_MAX] = '\0';
			previousfreq = freq;
		}
		callnr = batch_start + (int)batch_count;
	}

    close_summary_file();
	sustainedgoalelapsed_ms = elapsed_milliseconds(sustainedgoalstart_ms,
			timestamp_milliseconds());
	if (sustainedgoal && speed < sessiongoalspeed) {
		sustainedgoalspeedviolated = 1;
	}
	sustainedgoalmet = sustainedgoal && !stop_attempt &&
			qrq_practice_sustained_goal_met(sessiongoalspeed, sessiongoalduration,
					sustainedgoalelapsed_ms, sustainedgoalspeedviolated);
	attemptaccuracy = qrq_practice_accuracy((size_t)completedcalls, (size_t)errornr);
	sessioneligible = qrq_practice_session_eligible(attemptvalid,
			(size_t)completedcalls, (size_t)sessionlength, attemptaccuracy,
			accuracytarget);
	toplist_score = sessioneligible ? score : 0;
	if (qrq_history_append(historyfilename, &(struct qrq_history_entry){
			.timestamp = time(NULL), .callsign = mycall, .calls = completedcalls,
			.errors = errornr, .score = score, .max_speed = maxspeed,
			.eligible = sessioneligible}) != 0) {
		fprintf(stderr, "Unable to record session history in %s.\n", historyfilename);
	}

	/* attempt is over, send AR */
	callnr = 0;
	
	wait_morse_thread();
	start_morse_thread("+");
	
	add_to_toplist(mycall, toplist_score, maxspeed);
	
	curs_set(0);
	wattron(bot_w,A_BOLD);
	mvwaddstr(bot_w,1,1, "                                                          ");
	if (sustainedgoalmet) {
		mvwprintw(bot_w,1,1, "Goal met: %d CpM for %d sec. Raise it next time!",
				sessiongoalspeed, sessiongoalduration);
	}
	else if (sustainedgoal && sustainedgoalspeedviolated) {
		mvwprintw(bot_w,1,1, "Goal missed: speed fell below %d CpM. Reduce it by 25.",
				sessiongoalspeed);
	}
	else if (sustainedgoal) {
		mvwprintw(bot_w,1,1, "Goal stopped: restart and hold %d CpM for %d sec.",
				sessiongoalspeed, sessiongoalduration);
	}
	else if (accuracytarget != 0) {
		mvwprintw(bot_w,1,1, "Accuracy %d%% (goal %d%%). %s",
				attemptaccuracy, accuracytarget,
				attemptaccuracy >= accuracytarget ? "Raise it next time!" :
				"Repeat the same goal.");
	}
	else {
		mvwprintw(bot_w,1,1, "Attempt finished. Press any key to continue!");
	}
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
	int handled;


	/* Configuration values are read by the audio worker.  Join it before
	 * displaying or changing those values, and after each F6 sample. */
	wait_morse_thread();
	parameter_page = 0;
update_parameter_dialog();

while ((j = getch()) != 0) {
	handled = 0;
	if (j == '\t') {
		parameter_page = (parameter_page + 1) % 3;
		handled = 1;
	} else if (parameter_page == 1) {
			switch (j) {
				case 'v': volume = volume >= 5 ? volume - 5 : 0; handled = 1; break;
				case 'V': volume = volume <= 95 ? volume + 5 : 100; handled = 1; break;
				case 'n': qrnlevel = qrnlevel >= 5 ? qrnlevel - 5 : 0; handled = 1; break;
				case 'N': qrnlevel = qrnlevel <= 95 ? qrnlevel + 5 : 100; handled = 1; break;
				case 'r': qrmlevel = qrmlevel >= 5 ? qrmlevel - 5 : 0; handled = 1; break;
				case 'R': qrmlevel = qrmlevel <= 95 ? qrmlevel + 5 : 100; handled = 1; break;
				case 'q': qsblevel = qsblevel >= 5 ? qsblevel - 5 : 0; handled = 1; break;
				case 'Q': qsblevel = qsblevel <= 95 ? qsblevel + 5 : 100; handled = 1; break;
			case 'h':
				if (minpitch < maxpitch) {
					minpitch = minpitch <= maxpitch - 10 ? minpitch + 10 : maxpitch;
				}
				handled = 1;
				break;
			case 'H': if (minpitch > 100) minpitch -= 10; handled = 1; break;
			case 'j': if (maxpitch < 4000) maxpitch += 10; handled = 1; break;
			case 'J':
				if (maxpitch > minpitch) {
					maxpitch = maxpitch >= minpitch + 10 ? maxpitch - 10 : minpitch;
				}
				handled = 1;
				break;
#ifdef OSS
			case 'e':
				p = 0;
				readline(conf_w, 8, 25, dspdevice, CAPITALS_OFF,
						sizeof(dspdevice), 27, 1);
				if (dspdevice[0] == '\0') {
					strcpy(dspdevice, "/dev/dsp");
				}
				p = 0;
				handled = 1;
				break;
#endif
		}
	} else if (parameter_page == 2) {
		switch (j) {
			case '1': if (mincalllength > 1) mincalllength--; handled = 1; break;
			case '2': if (mincalllength < maxcalllength) mincalllength++; handled = 1; break;
			case '3': if (maxcalllength > mincalllength) maxcalllength--; handled = 1; break;
			case '4': if (maxcalllength < CALL_MAX) maxcalllength++; handled = 1; break;
			case 'i': digitmode = (digitmode + 1) % 3; handled = 1; break;
			case 'p': portablemode = (portablemode + 1) % 3; handled = 1; break;
			case 'P': portablevariants = portablevariants ? 0 : 1; handled = 1; break;
			case 'd':
				if (!callnr) {
					curs_set(1);
					callbase_dialog();
				}
				handled = 1;
				break;
			case 'x':
				p = 0;
				readline(conf_w, 6, 25, callprefixes, CAPITALS_ON,
						sizeof(callprefixes), 25, 0);
				p = 0;
				handled = 1;
				break;
			case 'y':
				p = 0;
				readline(conf_w, 7, 25, allowedchars, CAPITALS_ON,
						sizeof(allowedchars), 25, 0);
				p = 0;
				handled = 1;
				break;
			case 'z': {
				char seed_text[16];
				unsigned int parsed_seed;

				if (callnr) {
					handled = 1;
					break;
				}
				(void)snprintf(seed_text, sizeof(seed_text), "%u", sessionseed);
				p = 0;
				readline(conf_w, 8, 25, seed_text, CAPITALS_OFF,
						sizeof(seed_text), 15, 0);
				if (qrq_config_parse_uint(seed_text, &parsed_seed) == 0) {
					sessionseed = parsed_seed;
				}
				p = 0;
				handled = 1;
				break;
			}
			case 'h': focusconfusions = (focusconfusions ? 0 : 1); handled = 1; break;
			case 'm': spacedrepetition = (spacedrepetition ? 0 : 1); handled = 1; break;
			case 'b':
				answerbatch = answerbatch == QRQ_PRACTICE_MAX_ANSWER_BATCH ? 1 :
						answerbatch + 1;
				handled = 1;
				break;
			case 's':
				if (serialdigits == 0) serialdigits = QRQ_SERIAL_DIGITS_MIN;
				else if (serialdigits < QRQ_SERIAL_DIGITS_MAX) serialdigits++;
				else serialdigits = 0;
				handled = 1;
				break;
		}
	}
	if (!handled && parameter_page != 0 && j != KEY_F(2) &&
			j != KEY_F(6) && j != KEY_F(10) && j != KEY_F(3)) {
		handled = 1;
	}

	if (!handled) switch ((int) j) {
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
		case 'a':
				adaptiveselection = (adaptiveselection ? 0 : 1);
			break;
		case 'r':
				reviewmisses = (reviewmisses ? 0 : 1);
			break;
		case 'g':
			if (accuracytarget == 0) accuracytarget = 80;
			else if (accuracytarget == 80) accuracytarget = 90;
			else if (accuracytarget == 90) accuracytarget = 95;
			else if (accuracytarget == 95) accuracytarget = 98;
			else if (accuracytarget == 98) accuracytarget = 100;
			else accuracytarget = 0;
			break;
		case 'o':
			if (goalspeed == 0) goalspeed = initialspeed;
			else if (goalspeed <= QRQ_SPEED_MAX - 25) goalspeed += 25;
			else goalspeed = 0;
			break;
		case 'O':
			if (goalduration == 0) goalduration = 60;
			else if (goalduration == 60) goalduration = 180;
			else if (goalduration == 180) goalduration = 300;
			else if (goalduration == 300) goalduration = 600;
			else goalduration = 0;
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
			if (initialspeed <= QRQ_SPEED_MAX - 10) {
				initialspeed += 10;
			}
			break;
		case KEY_DOWN:
			if (initialspeed > 10) {
				initialspeed -= 10;
			}
			break;
		case KEY_RIGHT:
			if (mincharspeed <= QRQ_SPEED_MAX - 10) {
				mincharspeed += 10;
			}
			break;
		case KEY_LEFT:
			if (mincharspeed >= 10) {
				mincharspeed -= 10;
			}
			break;
		case KEY_PPAGE:
			if (speedupstep <= QRQ_SPEED_MAX - 2) {
				speedupstep += 2;
			}
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
			if (speeddownstep <= QRQ_SPEED_MAX - 2) {
				speeddownstep += 2;
			}
			break;
		case 'c':
			readline(conf_w, 6, 25, mycall, CAPITALS_ON, sizeof(mycall), 8, 0);
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
			readline(conf_w, 14, 25, dspdevice, CAPITALS_OFF,
					sizeof(dspdevice), 27, 1);
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
			if (save_config() == 0) {
				mvwprintw(conf_w,15,23, "  Config saved! ");
			} else {
				mvwprintw(conf_w,15,23, "  Save failed!  ");
			}
			wrefresh(conf_w);
#ifdef WIN32
			Sleep(1000);
#else
			sleep(1);	
#endif
			break;
		case KEY_F(6):
			freq = constanttone ? ctonefreq : 800;
			wait_morse_thread();
			start_morse_thread("TESTING");
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

	/* Initial speed is a next-session setting; do not reset an active
	 * session's adaptive speed merely because another option changed. */
	if (!callnr) {
		speed = initialspeed;
	}

	/* Once a running session uses a non-comparable option, changing the
	 * option back must not make that session eligible again. */
	if (!callnr) {
		attemptvalid = 1;
	}
	if (f6 || fixspeed || unlimitedattempt || sessionlength != 50 || adaptiveselection || reviewmisses || focusconfusions || spacedrepetition || answerbatch != 1 || serialdigits != 0 || portablevariants || sessionseed != 0 || qrq_practice_sustained_goal_active(goalspeed, goalduration)) {
		attemptvalid = 0;	
	}

	update_parameter_dialog();
	wait_morse_thread();

} /* while 1 (return only by F3/F10) */

} /* parameter_dialog */


/* update_parameter_dialog 
 * repaints the whole config/parameter screen (F5) */


void update_parameter_dialog (void) {
	static const char *const filter_mode_names[] = {"any", "required", "excluded"};
	char session_value[16];

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
	if (parameter_page == 1) {
		wattron(conf_w, A_BOLD);
		mvwaddstr(conf_w, 1, 1, "Configuration 2/3: Audio       Value       Change");
		mvwprintw(conf_w, 15, 2, "F6                   F2                F10");
		wattroff(conf_w, A_BOLD);
		mvwprintw(conf_w, 2, 2, "Output volume:              %3d %%       v/V -/+", volume);
		mvwprintw(conf_w, 3, 2, "QRN noise level:            %3d %%       n/N -/+", qrnlevel);
		mvwprintw(conf_w, 4, 2, "QRM CW level:              %3d %%       r/R -/+", qrmlevel);
		mvwprintw(conf_w, 5, 2, "QSB fade depth:            %3d %%       q/Q -/+", qsblevel);
		mvwprintw(conf_w, 6, 2, "Random pitch minimum:      %4d Hz      H/h -/+", minpitch);
		mvwprintw(conf_w, 7, 2, "Random pitch maximum:      %4d Hz      J/j -/+", maxpitch);
		mvwprintw(conf_w, 8, 2, "Audio sample rate:       %6ld Hz      qrqrc", samplerate);
		mvwprintw(conf_w, 9, 2, "Rise/fall and waveform controls are on page 1.");
#ifdef OSS
		mvwprintw(conf_w, 10, 2, "DSP device: %-27.27s e", dspdevice);
#endif
		mvwprintw(conf_w, 15, 4, ": Play CW sample");
		mvwprintw(conf_w, 15, 25, ": Save config");
		mvwprintw(conf_w, 15, 44, ": Exit");
		mvwaddstr(inf_w, 1, 1, "Tab: next settings page   lowercase/uppercase: -/+");
		wrefresh(conf_w);
		wrefresh(inf_w);
		return;
	}
	if (parameter_page == 2) {
		char serial_mode[16];

		if (serialdigits == 0) {
			(void)snprintf(serial_mode, sizeof(serial_mode), "off");
		} else {
			(void)snprintf(serial_mode, sizeof(serial_mode), "%d digits", serialdigits);
		}
		wattron(conf_w, A_BOLD);
		mvwaddstr(conf_w, 1, 1, "Configuration 3/3: Call filters       Value / Change");
		mvwprintw(conf_w, 15, 2, "F6                   F2                F10");
		wattroff(conf_w, A_BOLD);
		mvwprintw(conf_w, 2, 2, "Minimum call length:      %2d       1/2 -/+", mincalllength);
		mvwprintw(conf_w, 3, 2, "Maximum call length:      %2d       3/4 -/+", maxcalllength);
		mvwprintw(conf_w, 4, 2, "Digits:                   %-8s i cycle",
				filter_mode_names[digitmode]);
		mvwprintw(conf_w, 5, 2, "Portable suffix: %-8s p  Variants*: %-3s P",
				filter_mode_names[portablemode], portablevariants ? "yes" : "no");
		mvwprintw(conf_w, 6, 2, "Call prefixes: %-25.25s x", callprefixes[0] ? callprefixes : "(any)");
		mvwprintw(conf_w, 7, 2, "Allowed chars: %-25.25s y", allowedchars[0] ? allowedchars : "(any)");
		mvwprintw(conf_w, 8, 2, "Serial exchanges*:        %-10s s", serial_mode);
		mvwprintw(conf_w, 9, 2, "Session seed*:            %-10u %s", sessionseed,
				callnr ? "next session" : "z");
		mvwprintw(conf_w, 10, 2, "Focus confusions*:        %-3s h",
				focusconfusions ? "yes" : "no");
		mvwprintw(conf_w, 11, 2, "Spaced review*:           %-3s m",
				spacedrepetition ? "yes" : "no");
		mvwprintw(conf_w, 12, 2, "Answer batch*:            %-3d b", answerbatch);
		if (serialdigits != 0) {
			mvwaddstr(conf_w, 13, 2,
					"Serial generator overrides callsign database.");
		} else if (portablevariants) {
			mvwaddstr(conf_w, 13, 2,
					"Portable generator overrides the suffix filter.");
		} else if (!callnr) {
			mvwprintw(conf_w, 13, 2, "Callsign database: %-24.24s d", basename(cbfilename));
		}
		mvwaddstr(conf_w, 14, 2, "Filters and training modes apply to the next session.");
		mvwprintw(conf_w, 15, 4, ": Play CW sample");
		mvwprintw(conf_w, 15, 25, ": Save config");
		mvwprintw(conf_w, 15, 44, ": Exit");
		mvwaddstr(inf_w, 1, 1, "Tab: next settings page   * Toplist-ineligible");
		wrefresh(conf_w);
		wrefresh(inf_w);
		return;
	}
	wattron(conf_w,A_BOLD);
	mvwaddstr(conf_w,1,1, "Configuration 1/3:      Value                Change");
	mvwprintw(conf_w,15,2, "F6                   F2                F10");
	wattroff(conf_w, A_BOLD);
	mvwprintw(conf_w,2,2, "Initial Speed:         %3d CpM / %3d WpM" 
					"    up/down", initialspeed, initialspeed/5);
	mvwprintw(conf_w,3,2, "Character speed floor: %3d CpM / %3d WpM"
					" left/right", mincharspeed, mincharspeed/5);
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
	mvwprintw(conf_w,10,2, "Fixed speed*: %-3s s  Stop on error: %-3s t",
			fixspeed ? "yes" : "no", stoponerror ? "yes" : "no");
	if (unlimitedattempt) {
		strcpy(session_value, "all");
	}
	else {
		(void)snprintf(session_value, sizeof(session_value), "%d", sessionlength);
	}
	mvwprintw(conf_w,11,2, "Session calls*:        %-10s [ / ] or u", session_value);
	if (!callnr) {
		mvwprintw(conf_w,12,2, "Callsign database:     %-15s"
					"      d (%zu)", basename(cbfilename),nrofcalls);
	}
	mvwprintw(conf_w,13,2, "Adaptive*: %-3s Review*: %-3s Accuracy*: %-3d%% a/r/g",
			adaptiveselection ? "yes" : "no", reviewmisses ? "yes" : "no",
			accuracytarget);
	mvwprintw(conf_w,14,2, "Sustain goal*: %4d CpM / %4d sec      o/O",
			goalspeed, goalduration);
	mvwprintw(conf_w,15,4, ": Play CW sample");
	mvwprintw(conf_w,15,25, ": Save config");
	mvwprintw(conf_w,15,44, ": Exit");
	mvwprintw(inf_w,1,1, "Tab: next page  * Ineligible  Speed maximum: %d",
			QRQ_SPEED_MAX);
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














/* Draw a bounded horizontal viewport around the shared input cursor. */
static void draw_input_line(WINDOW *win, int y, int x, const char *line,
		int display_width) {
	size_t line_length = strlen(line);
	size_t view_start = 0;

	if (p < 0) p = 0;
	if ((size_t)p > line_length) p = (int)line_length;
	if (p >= display_width) {
		view_start = (size_t)(p - display_width + 1);
	}
	for (int column = 0; column < display_width; ++column) {
		mvwaddch(win, y, x + column, ' ');
	}
	(void)mvwaddnstr(win, y, x, line + view_start, display_width);
	wmove(win, y, x + p - (int)view_start);
}

/* Edit a bounded string, optionally accepting printable path characters. */
static int readline(WINDOW *win, int y, int x, char *line, int capitals,
		size_t capacity, int display_width, int path_mode) {
	int c;						/* character we read */
	int i=0;
	size_t line_len;
	int accepted_character;

	if (win == NULL || line == NULL || capacity == 0 || display_width <= 0) {
		return 0;
	}

	if (strlen(line) == 0) {p=0;}	/* cursor to start if no call in buffer */
	
	if (mode == 1) { 
		mvwaddstr(win,1,55,"INS");
	}
	else {
		mvwaddstr(win,1,55,"OVR");
	}

	draw_input_line(win, y, x, line, display_width);
	wrefresh(win);
	curs_set(TRUE);
	
	while (1) {
		c = wgetch(win);
#ifndef WIN32
		if (resize_pending) {
			apply_terminal_resize();
			continue;
		}
#endif
		if (c == '\n' && is_sending_complete())
			break;
		line_len = strlen(line);

		accepted_character = validchar(c) || (path_mode && c >= 32 && c <= 126 && c != '#');
		if (accepted_character &&
				((mode == 0 && p < (int)line_len) || line_len + 1 < capacity)) {

            /* Accept - as / for layouts where / requires Shift, except when
			 * editing a path where a literal hyphen is meaningful. */
            if (!path_mode && c == '-') {
                c = '/';
            }

			if (capitals) {
				c = toupper((unsigned char)c);
			}
			if (mode == 1) {						/* insert */
				memmove(line + p + 1, line + p, line_len - (size_t)p + 1);
			} else if (p == (int)line_len) {
				line[line_len + 1] = '\0';
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
			if (speed <= QRQ_SPEED_MAX - 5) {
				speed += 5;
			}
			update_score();
			wrefresh(top_w);
		}
		else if (c == KEY_NPAGE && callnr && !attemptvalid) {
			if (speed > 20) {
				speed = speed >= 25 ? speed - 5 : 20;
			}
			update_score();
			wrefresh(top_w);
		}
		else if (c == KEY_F(5) && win != conf_w) {
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
			wait_morse_thread();
			start_morse_thread("73");
			wait_morse_thread();
			exit(0);
		}

		draw_input_line(win, y, x, line, display_width);
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
	if (fseek(fh, 0, SEEK_SET) != 0) {
		fclose(fh);
		return -1;
	}
	if (fgets(tmp, 34, fh) == NULL && ferror(fh)) {
		fclose(fh);
		return -1;
	}
	while (i < 21 && fgets(tmp, 34, fh) != NULL) {
		tmp[17]='\0';
		if (toplist_own) {
			if (qrq_toplist_callsign_matches(tmp, mycall)) {
				mvwaddstr(right_w,i+2, 2, tmp);
				i++;
			}
		}
		else {
			if (qrq_toplist_callsign_matches(tmp, mycall)) {
				wattron(right_w, A_BOLD);
			}
			mvwaddstr(right_w,i+2, 2, tmp);
			i++;
			wattroff(right_w, A_BOLD);
		}
	}
	if (ferror(fh)) {
		fclose(fh);
		return -1;
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
	if (fclose(fh) != 0) {
		return -1;
	}
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
	int summary_fd;
	int write_failed;

    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL) {
        return;
    }

    if (strftime(time_fmt, sizeof(time_fmt), "%Y%m%d_%H%M%S", tmp) == 0) {
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
	if (path_len > SIZE_MAX - call_len - time_len - 14) {
		fprintf(stderr, "Summary filename is too long.\n");
		return;
	}
	filename_len = path_len + call_len + time_len + 14;
	filename = malloc(filename_len);
	if (filename == NULL) {
		fprintf(stderr, "Out of memory while creating summary filename.\n");
		return;
	}
	(void)snprintf(filename, filename_len, "%s/%s-%s.txt.XXXXXX", sumfilepath,
			mycall, time_fmt);

	summary_fd = mkstemp(filename);
	if (summary_fd == -1) {
		printf("Unable to open summary file (%s)!\r\n", filename);
		free(filename);
		return;
	}
	fh = fdopen(summary_fd, "wb");
	if (fh == NULL) {
		int saved_errno = errno;
		close(summary_fd);
		unlink(filename);
		fprintf(stderr, "Unable to open summary stream (%s): %s\n", filename,
				strerror(saved_errno));
		free(filename);
		return;
	}

	write_failed = fwrite(summary, 1, s_pos, fh) != s_pos;
	if (fclose(fh) != 0) {
		write_failed = 1;
	}
	if (write_failed) {
		fprintf(stderr, "Unable to complete summary file (%s)!\n", filename);
		unlink(filename);
		free(filename);
		return;
	}
	
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
	mvwaddstr(top_w,2,1, "                                                          ");
	if (attemptvalid) {
		mvwprintw(top_w, 1, 27, "%6d", score);	
	}
	else if (focusconfusions_active) {
		mvwprintw(top_w, 1, 27, "[focus drills]");
	}
	else if (answerbatch > 1) {
		mvwprintw(top_w, 1, 27, "[batch copy: %d]", answerbatch);
	}
	else if (qrq_practice_sustained_goal_active(goalspeed, goalduration)) {
		mvwprintw(top_w, 1, 27, "[sustain: %d/%ds]", goalspeed,
				goalduration);
	}
	else if (serialdigits != 0) {
		mvwprintw(top_w, 1, 27, "[serials: %d digits]", serialdigits);
	}
	else if (spacedrepetition) {
		if (spaced_due_count != 0) {
			mvwprintw(top_w, 1, 27, "[spaced: %zu due]", spaced_due_count);
		} else {
			mvwprintw(top_w, 1, 27, "[spaced review]");
		}
	}
	else {
		mvwprintw(top_w, 1, 27, "[training mode]");	
	}
	mvwprintw(top_w, 2, 1, "Overall:%3d CpM/%3d WpM  Char:%3d CpM  Max:%3d CpM",
			speed, speed/5, speed < mincharspeed ? mincharspeed : speed, maxspeed);
	wrefresh(top_w);
	return 0;
}

/* display the correct callsign and what the user entered, with mistakes
 * highlighted. */
static int show_error (char * realcall, char * wrongcall) {
	int x=2;
	int y;
	size_t display_slot;

	// when call_maxlen <= CALL_MAX/2, we are showing the errors in two columns, otherwise just one.
	int max_nr_err = call_maxlen <= CALL_MAX/2 ? 30 : 15;   
	int max_disp_len = call_maxlen <= CALL_MAX/2 ? CALL_MAX/2 : CALL_MAX;

	// cut entered call if it's longer than what we can display
	if (strlen(wrongcall) > (size_t)max_disp_len) {
		wrongcall[max_disp_len] = '\0';
	}

	display_slot = displayed_errors % (size_t)max_nr_err;
	if (display_slot == 0 && displayed_errors != 0) {
		clear_display();
	}
	displayed_errors++;
	display_slot++;
	y = (int)display_slot;

	/* Move to second column after 15 errors if applicable */	
	if (max_nr_err == 30 && display_slot > 15) {
		x=30;
		y = (int)display_slot - 15;
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

#ifndef WIN32
static void note_terminal_resize(int signal_number) {
	(void)signal_number;
	resize_pending = 1;
}

static void apply_terminal_resize(void) {
	if (!resize_pending) {
		return;
	}
	resize_pending = 0;
	resizeterm(0, 0);
	if (LINES < 24 || COLS < 80) {
		endwin();
		fprintf(stderr, "QRQ requires a terminal of at least 80 columns by 24 rows "
				"(current: %d by %d).\n", COLS, LINES);
		exit(EXIT_FAILURE);
	}
	clearok(stdscr, TRUE);
	if (top_w != NULL) touchwin(top_w);
	if (mid_w != NULL) touchwin(mid_w);
	if (conf_w != NULL) touchwin(conf_w);
	if (bot_w != NULL) touchwin(bot_w);
	if (inf_w != NULL) touchwin(inf_w);
	if (right_w != NULL) touchwin(right_w);
}
#endif

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
	if (score < 0 || score > QRQ_SESSION_SCORE_MAX) {
		fprintf(stderr, "Toplist score is outside the supported range.\n");
		return -1;
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


static void config_value_error(int line, const char *key, const char *value) {
	printw("  line %3d: invalid %s value >%s<; keeping previous value.\n",
			line, key, value);
}

static int config_int_value(int line, const char *key, const char *value,
		int minimum, int maximum, int *destination) {
	int parsed;

	if (qrq_config_parse_int(value, minimum, maximum, &parsed) != 0) {
		config_value_error(line, key, value);
		return -1;
	}
	*destination = parsed;
	return 0;
}

static int config_double_value(int line, const char *key, const char *value,
		double minimum, double maximum, double *destination) {
	double parsed;

	if (qrq_config_parse_double(value, minimum, maximum, &parsed) != 0) {
		config_value_error(line, key, value);
		return -1;
	}
	*destination = parsed;
	return 0;
}

static int config_string_value(int line, const char *key, const char *value,
		char *destination, size_t capacity, int uppercase) {
	if (qrq_config_copy_string(value, destination, capacity, uppercase) != 0) {
		config_value_error(line, key, value);
		return -1;
	}
	return 0;
}

static int config_graph_string_value(int line, const char *key,
		const char *value, char *destination, size_t capacity) {
	const unsigned char *cursor = (const unsigned char *)value;

	while (*cursor != '\0') {
		if (!isgraph(*cursor)) {
			config_value_error(line, key, value);
			return -1;
		}
		cursor++;
	}
	return config_string_value(line, key, value, destination, capacity, 1);
}

static int config_callsign_value(int line, const char *value) {
	const unsigned char *cursor = (const unsigned char *)value;
	size_t length = strlen(value);

	if (length > 7) {
		config_value_error(line, "callsign", value);
		return -1;
	}
	while (*cursor != '\0') {
		if (!isalnum(*cursor) && *cursor != '/') {
			config_value_error(line, "callsign", value);
			return -1;
		}
		cursor++;
	}
	return config_string_value(line, "callsign", value, mycall,
			sizeof(mycall), 1);
}

static int read_config(void) {
	char *line_buffer = NULL;
	char *key;
	char *value;
	double parsed_double;
	FILE *fh;
	int legacy_step = speedupstep;
	int parsed_int;
	int line = 0;
	int line_status;
	int split_status;
	int have_legacy_step = 0;
	int have_speedup_step = 0;
	int have_speeddown_step = 0;
	int configured_speedup_step = speedupstep;
	int configured_speeddown_step = speeddownstep;
	int configured_minpitch = minpitch;
	int configured_maxpitch = maxpitch;
	int configured_mincalllength = mincalllength;
	int configured_maxcalllength = maxcalllength;
	size_t capacity = 0;
	unsigned int parsed_seed;

	fh = fopen(rcfilename, "r");
	if (fh == NULL) {
		endwin();
		fprintf(stderr, "Unable to open config file %s: %s\n",
				rcfilename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	while ((line_status = qrq_config_read_line(fh, &line_buffer, &capacity)) == 1) {
		line++;
		split_status = qrq_config_split_line(line_buffer, &key, &value);
		if (split_status == 0) {
			continue;
		}
		if (split_status < 0) {
			printw("  line %3d: malformed configuration line ignored.\n", line);
			continue;
		}

		if (strcmp(key, "callsign") == 0) {
			(void)config_callsign_value(line, value);
		} else if (strcmp(key, "initialspeed") == 0) {
			(void)config_int_value(line, key, value, QRQ_SPEED_MIN,
					QRQ_SPEED_MAX, &initialspeed);
		} else if (strcmp(key, "mincharspeed") == 0) {
			(void)config_int_value(line, key, value, 0, QRQ_SPEED_MAX,
					&mincharspeed);
		} else if (strcmp(key, "speedstep") == 0) {
			if (config_int_value(line, key, value, 1, 5000, &parsed_int) == 0) {
				legacy_step = parsed_int;
				have_legacy_step = 1;
			}
		} else if (strcmp(key, "speedupstep") == 0) {
			if (config_int_value(line, key, value, 1, 5000, &parsed_int) == 0) {
				configured_speedup_step = parsed_int;
				have_speedup_step = 1;
			}
		} else if (strcmp(key, "speeddownstep") == 0) {
			if (config_int_value(line, key, value, 1, 5000, &parsed_int) == 0) {
				configured_speeddown_step = parsed_int;
				have_speeddown_step = 1;
			}
		} else if (strcmp(key, "dspdevice") == 0) {
			(void)config_string_value(line, key, value, dspdevice,
					sizeof(dspdevice), 0);
		} else if (strcmp(key, "risetime") == 0) {
			if (config_double_value(line, key, value, 0.1, 10.0,
					&parsed_double) == 0) {
				edge = parsed_double;
			}
		} else if (strcmp(key, "waveform") == 0) {
			(void)config_int_value(line, key, value, SINE, SQUARE, &waveform);
		} else if (strcmp(key, "constanttone") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &constanttone);
		} else if (strcmp(key, "ctonefreq") == 0) {
			(void)config_int_value(line, key, value, 100, 1600, &ctonefreq);
		} else if (strcmp(key, "volume") == 0) {
			(void)config_int_value(line, key, value, 0, 100, &volume);
		} else if (strcmp(key, "qrnlevel") == 0) {
			(void)config_int_value(line, key, value, 0, 100, &qrnlevel);
		} else if (strcmp(key, "qsblevel") == 0) {
			(void)config_int_value(line, key, value, 0, 100, &qsblevel);
		} else if (strcmp(key, "qrmlevel") == 0) {
			(void)config_int_value(line, key, value, 0, 100, &qrmlevel);
		} else if (strcmp(key, "minpitch") == 0) {
			(void)config_int_value(line, key, value, 100, 4000,
					&configured_minpitch);
		} else if (strcmp(key, "maxpitch") == 0) {
			(void)config_int_value(line, key, value, 100, 4000,
					&configured_maxpitch);
		} else if (strcmp(key, "f6") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &f6);
		} else if (strcmp(key, "fixspeed") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &fixspeed);
		} else if (strcmp(key, "stoponerror") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &stoponerror);
		} else if (strcmp(key, "unlimitedattempt") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &unlimitedattempt);
		} else if (strcmp(key, "sessionlength") == 0) {
			(void)config_int_value(line, key, value, 1, 1000000, &sessionlength);
		} else if (strcmp(key, "adaptiveselection") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &adaptiveselection);
		} else if (strcmp(key, "reviewmisses") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &reviewmisses);
		} else if (strcmp(key, "focusconfusions") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &focusconfusions);
		} else if (strcmp(key, "spacedrepetition") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &spacedrepetition);
		} else if (strcmp(key, "answerbatch") == 0) {
			(void)config_int_value(line, key, value, 1,
					QRQ_PRACTICE_MAX_ANSWER_BATCH, &answerbatch);
		} else if (strcmp(key, "serialdigits") == 0) {
			if (config_int_value(line, key, value, 0,
					QRQ_SERIAL_DIGITS_MAX, &parsed_int) == 0) {
				if (parsed_int == 0 || parsed_int >= QRQ_SERIAL_DIGITS_MIN) {
					serialdigits = parsed_int;
				} else {
					config_value_error(line, key, value);
				}
			}
		} else if (strcmp(key, "callprefixes") == 0) {
			(void)config_graph_string_value(line, key, value, callprefixes,
					sizeof(callprefixes));
		} else if (strcmp(key, "digitmode") == 0) {
			(void)config_int_value(line, key, value, 0, 2, &digitmode);
		} else if (strcmp(key, "portablemode") == 0) {
			(void)config_int_value(line, key, value, 0, 2, &portablemode);
		} else if (strcmp(key, "portablevariants") == 0) {
			(void)config_int_value(line, key, value, 0, 1, &portablevariants);
		} else if (strcmp(key, "allowedchars") == 0) {
			(void)config_graph_string_value(line, key, value, allowedchars,
					sizeof(allowedchars));
		} else if (strcmp(key, "accuracytarget") == 0) {
			if (config_int_value(line, key, value, 0, 100, &parsed_int) == 0) {
				if (parsed_int == 0 || parsed_int >= 50) {
					accuracytarget = parsed_int;
				} else {
					config_value_error(line, key, value);
				}
			}
		} else if (strcmp(key, "goalspeed") == 0) {
			if (config_int_value(line, key, value, 0, QRQ_SPEED_MAX,
					&parsed_int) == 0) {
				if (parsed_int == 0 || parsed_int >= QRQ_SPEED_MIN) {
					goalspeed = parsed_int;
				} else {
					config_value_error(line, key, value);
				}
			}
		} else if (strcmp(key, "goalduration") == 0) {
			(void)config_int_value(line, key, value, 0,
					QRQ_PRACTICE_SUSTAINED_GOAL_MAX_SECONDS, &goalduration);
		} else if (strcmp(key, "sessionseed") == 0) {
			if (qrq_config_parse_uint(value, &parsed_seed) == 0) {
				sessionseed = parsed_seed;
			} else {
				config_value_error(line, key, value);
			}
		} else if (strcmp(key, "mincalllength") == 0) {
			(void)config_int_value(line, key, value, 1, CALL_MAX,
					&configured_mincalllength);
		} else if (strcmp(key, "maxcalllength") == 0) {
			(void)config_int_value(line, key, value, 1, CALL_MAX,
					&configured_maxcalllength);
		} else if (strcmp(key, "callbase") == 0) {
			(void)config_string_value(line, key, value, cbfilename,
					sizeof(cbfilename), 0);
		} else if (strcmp(key, "samplerate") == 0) {
			if (config_int_value(line, key, value, 8000, 384000,
					&parsed_int) == 0) {
				samplerate = parsed_int;
			}
		} else {
			printw("  line %3d: unknown option >%s< ignored.\n", line, key);
		}
	}
	if (line_status < 0) {
		int saved_errno = errno;
		free(line_buffer);
		(void)fclose(fh);
		endwin();
		fprintf(stderr, "Unable to read config file %s: %s\n",
				rcfilename, strerror(saved_errno));
		exit(EXIT_FAILURE);
	}
	free(line_buffer);
	if (fclose(fh) != 0) {
		endwin();
		fprintf(stderr, "Unable to close config file %s: %s\n",
				rcfilename, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (have_legacy_step) {
		speedupstep = legacy_step;
		speeddownstep = legacy_step;
	}
	if (have_speedup_step) {
		speedupstep = configured_speedup_step;
	}
	if (have_speeddown_step) {
		speeddownstep = configured_speeddown_step;
	}
	if (configured_minpitch <= configured_maxpitch) {
		minpitch = configured_minpitch;
		maxpitch = configured_maxpitch;
	} else {
		printw("  minpitch must not exceed maxpitch; keeping %d..%d Hz.\n",
				minpitch, maxpitch);
	}
	if (configured_mincalllength <= configured_maxcalllength) {
		mincalllength = configured_mincalllength;
		maxcalllength = configured_maxcalllength;
	} else {
		printw("  mincalllength must not exceed maxcalllength; keeping %d..%d.\n",
				mincalllength, maxcalllength);
	}
	speed = initialspeed;
	printw("Finished reading qrqrc.\n");
	return 0;
}
static MORSE_THREAD_RETURN morse(void *arg) {
	const char *text = arg;
	int i,j;
	int c, fulldotlen, dotlen, dashlen, charspeed, farnsworth, fwdotlen;
	const char *code;

#if WIN32 /* WinMM simple support by Lukasz Komsta, SP8QED */
	HWAVEOUT h = NULL;
	WAVEFORMATEX wf = {0};
	WAVEHDR wh = {0};
	HANDLE d = NULL;
	MMRESULT winmm_result;
	int header_prepared = 0;
	int wave_opened = 0;

	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 1;
	wf.wBitsPerSample = 16;
	wf.nSamplesPerSec = samplerate * 2;
	wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	wf.cbSize = 0;
	d = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (d == NULL) {
		fprintf(stderr, "Unable to create WinMM playback event (error %lu).\n",
				(unsigned long)GetLastError());
		goto audio_error;
	}
	winmm_result = waveOutOpen(&h, WAVE_MAPPER, &wf, (DWORD_PTR)d, 0,
			CALLBACK_EVENT);
	if (winmm_result != MMSYSERR_NOERROR) {
		fprintf(stderr, "Unable to open WinMM output (error %u).\n",
				(unsigned int)winmm_result);
		goto audio_error;
	}
	wave_opened = 1;

#else
	/* opening the DSP device */
	dsp_fd = open_dsp(dspdevice);
#endif

#ifdef PA
	if (dsp_fd == NULL) {
		set_sending_complete(1);
		return MORSE_THREAD_RESULT;
	}
#endif
	/* set bufpos to 0 */

	full_bufpos = 0; 
	qrm_dot_samples = (size_t)(samplerate * 6 /
			(speed < mincharspeed ? mincharspeed : speed));
	if (qrm_dot_samples == 0) {
		qrm_dot_samples = 1;
	}

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
	if (ed >= dotlen) {
		ed = dotlen - 1;
	}
	if (ed < 0) {
		ed = 0;
	}

	/* the signal needs "ed" samples to reach the full amplitude and
	 * at the end another "ed" samples to reach zero. The dots and
	 * dashes therefore are becoming longer by "ed" and the pauses
	 * after them are shortened accordingly by "ed" samples */

	for (i = 0; i < (int)strlen(text); i++) {
		c = text[i];
		if (c == ' ') {
			int spacing_unit = farnsworth ? fwdotlen : fulldotlen;

			/* The preceding character already contributed its three-unit
			 * character gap. Four more units make the standard seven-unit
			 * word gap. */
			if (tonegen(0, 4 * spacing_unit, SILENCE) != 0) {
				goto audio_error;
			}
			continue;
		}
		if (isalpha((unsigned char)c)) {
			code = codetable[c-65];
		}
		else if (isdigit((unsigned char)c)) {
			code = codetable[c-22];
		}
		else if (c == '/') { 
			code = "-..-.";
		}
		else if (c == '+') {
			code = ".-.-.";
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
	if (full_bufpos < 2 || full_bufpos - 2 > UINT32_MAX) {
		fprintf(stderr, "Generated audio is too large for WinMM.\n");
		goto audio_error;
	}
	wh.lpData = (char*) full_buf;
	wh.dwBufferLength = (DWORD) (full_bufpos - 2);
	wh.dwFlags = 0;
	wh.dwLoops = 0;
	winmm_result = waveOutPrepareHeader(h, &wh, sizeof(wh));
	if (winmm_result != MMSYSERR_NOERROR) {
		fprintf(stderr, "Unable to prepare WinMM audio (error %u).\n",
				(unsigned int)winmm_result);
		goto audio_error;
	}
	header_prepared = 1;
	if (!ResetEvent(d)) {
		fprintf(stderr, "Unable to reset WinMM playback event (error %lu).\n",
				(unsigned long)GetLastError());
		goto audio_error;
	}
	winmm_result = waveOutWrite(h, &wh, sizeof(wh));
	if (winmm_result != MMSYSERR_NOERROR) {
		fprintf(stderr, "Unable to start WinMM playback (error %u).\n",
				(unsigned int)winmm_result);
		goto audio_error;
	}
	if (WaitForSingleObject(d, INFINITE) != WAIT_OBJECT_0) {
		fprintf(stderr, "Unable to wait for WinMM playback (error %lu).\n",
				(unsigned long)GetLastError());
		goto audio_error;
	}
	winmm_result = waveOutUnprepareHeader(h, &wh, sizeof(wh));
	if (winmm_result != MMSYSERR_NOERROR) {
		fprintf(stderr, "Unable to release WinMM audio (error %u).\n",
				(unsigned int)winmm_result);
		goto audio_error;
	}
	header_prepared = 0;
	if (waveOutClose(h) != MMSYSERR_NOERROR) {
		fprintf(stderr, "Unable to close WinMM output.\n");
		goto audio_error;
	}
	h = NULL;
	wave_opened = 0;
	if (!CloseHandle(d)) {
		fprintf(stderr, "Unable to close WinMM playback event (error %lu).\n",
				(unsigned long)GetLastError());
		goto audio_error;
	}
	d = NULL;
#else
	{
		int write_result = write_audio(dsp_fd, full_buf, (int)full_bufpos);
		int close_result = close_audio(dsp_fd);

		if (write_result != 0 || close_result != 0) {
#ifdef OSS
			fprintf(stderr, "Audio playback failed: %s\n", strerror(errno));
#else
			fprintf(stderr, "Audio playback failed.\n");
#endif
			set_sending_complete(1);
			return MORSE_THREAD_RESULT;
		}
	}
#endif
	set_sending_complete(1);
	return MORSE_THREAD_RESULT;

audio_error:
#if WIN32
	if (wave_opened) {
		(void)waveOutReset(h);
		if (header_prepared) {
			(void)waveOutUnprepareHeader(h, &wh, sizeof(wh));
		}
		(void)waveOutClose(h);
	}
	if (d != NULL) {
		(void)CloseHandle(d);
	}
#elif defined(OSS) || defined(CA)
	(void) close_audio(dsp_fd);
#endif
	set_sending_complete(1);
	return MORSE_THREAD_RESULT;
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

static double qrn_sample(void) {
	/* LCG output is sufficient for audible noise and deliberately does not
	 * consume rand(), which drives reproducible practice selection. */
	qrn_state = qrn_state * 1664525U + 1013904223U;
	return ((double)qrn_state / (double)UINT32_MAX) * 2.0 - 1.0;
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
		if (qsblevel != 0) {
			double fade = (double)qsblevel / 100.0;

			val *= 1.0 - fade * (0.5 + 0.5 * sin(qsb_phase));
			qsb_phase += 2.0 * PI * 0.8 / (double)samplerate;
			if (qsb_phase >= 2.0 * PI) {
				qsb_phase -= 2.0 * PI;
			}
		}
		if (qrmlevel != 0) {
			val += qrq_qrm_next_sample(&qrm_state, (unsigned int)samplerate,
					qrm_dot_samples, qrmlevel);
		}
		
		if (qrnlevel != 0) {
			val += qrn_sample() * (qrnlevel / 100.0);
		}
		out = (int) (val * 32500.0 * volume / 100.0);
		if (out > 32500) out = 32500;
		if (out < -32500) out = -32500;
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
		"speedupstep", "speeddownstep", "sessionlength", "mincalllength",
		"maxcalllength", "stoponerror", "adaptiveselection", "reviewmisses",
		"accuracytarget", "callprefixes", "digitmode", "portablemode",
		"allowedchars", "sessionseed", "volume", "minpitch", "maxpitch",
		"qrnlevel", "qsblevel", "qrmlevel", "samplerate", "focusconfusions", "spacedrepetition",
		"answerbatch", "serialdigits", "goalspeed", "goalduration", "portablevariants"
	};
	FILE *fh = NULL;
	char tmp[PATH_MAX + 80];
	char *config = NULL;
	size_t config_len;
	long file_length;
	int i;
	int written;
	int result = -1;

	if ((fh = fopen(rcfilename, "rb")) == NULL) {
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

	/* Replace only full keys that begin a line, preserving comments and spacing. */
	for (i = 0; i < (int)(sizeof(confopts) / sizeof(confopts[0])); i++) {
		switch (i) {
			case 0: written = snprintf(tmp, sizeof(tmp), "%s", mycall); break;
			case 1: written = snprintf(tmp, sizeof(tmp), "%s", cbfilename); break;
			case 2: written = snprintf(tmp, sizeof(tmp), "%s", dspdevice); break;
			case 3: written = snprintf(tmp, sizeof(tmp), "%d", initialspeed); break;
			case 4: written = snprintf(tmp, sizeof(tmp), "%d", mincharspeed); break;
			case 5: written = snprintf(tmp, sizeof(tmp), "%d", waveform); break;
			case 6: written = snprintf(tmp, sizeof(tmp), "%d", constanttone); break;
			case 7: written = snprintf(tmp, sizeof(tmp), "%d", ctonefreq); break;
			case 8: written = snprintf(tmp, sizeof(tmp), "%d", fixspeed); break;
			case 9: written = snprintf(tmp, sizeof(tmp), "%d", unlimitedattempt); break;
			case 10: written = snprintf(tmp, sizeof(tmp), "%d", f6); break;
			case 11: written = snprintf(tmp, sizeof(tmp), "%f", edge); break;
			case 12: written = snprintf(tmp, sizeof(tmp), "%d", speedupstep); break;
			case 13: written = snprintf(tmp, sizeof(tmp), "%d", speedupstep); break;
			case 14: written = snprintf(tmp, sizeof(tmp), "%d", speeddownstep); break;
			case 15: written = snprintf(tmp, sizeof(tmp), "%d", sessionlength); break;
			case 16: written = snprintf(tmp, sizeof(tmp), "%d", mincalllength); break;
			case 17: written = snprintf(tmp, sizeof(tmp), "%d", maxcalllength); break;
			case 18: written = snprintf(tmp, sizeof(tmp), "%d", stoponerror); break;
			case 19: written = snprintf(tmp, sizeof(tmp), "%d", adaptiveselection); break;
			case 20: written = snprintf(tmp, sizeof(tmp), "%d", reviewmisses); break;
			case 21: written = snprintf(tmp, sizeof(tmp), "%d", accuracytarget); break;
			case 22: written = snprintf(tmp, sizeof(tmp), "%s", callprefixes); break;
			case 23: written = snprintf(tmp, sizeof(tmp), "%d", digitmode); break;
			case 24: written = snprintf(tmp, sizeof(tmp), "%d", portablemode); break;
			case 25: written = snprintf(tmp, sizeof(tmp), "%s", allowedchars); break;
			case 26: written = snprintf(tmp, sizeof(tmp), "%u", sessionseed); break;
			case 27: written = snprintf(tmp, sizeof(tmp), "%d", volume); break;
			case 28: written = snprintf(tmp, sizeof(tmp), "%d", minpitch); break;
			case 29: written = snprintf(tmp, sizeof(tmp), "%d", maxpitch); break;
			case 30: written = snprintf(tmp, sizeof(tmp), "%d", qrnlevel); break;
			case 31: written = snprintf(tmp, sizeof(tmp), "%d", qsblevel); break;
			case 32: written = snprintf(tmp, sizeof(tmp), "%d", qrmlevel); break;
			case 33: written = snprintf(tmp, sizeof(tmp), "%ld", samplerate); break;
			case 34: written = snprintf(tmp, sizeof(tmp), "%d", focusconfusions); break;
			case 35: written = snprintf(tmp, sizeof(tmp), "%d", spacedrepetition); break;
			case 36: written = snprintf(tmp, sizeof(tmp), "%d", answerbatch); break;
			case 37: written = snprintf(tmp, sizeof(tmp), "%d", serialdigits); break;
			case 38: written = snprintf(tmp, sizeof(tmp), "%d", goalspeed); break;
			case 39: written = snprintf(tmp, sizeof(tmp), "%d", goalduration); break;
			default: written = snprintf(tmp, sizeof(tmp), "%d", portablevariants); break;
		}
		if (written < 0 || (size_t)written >= sizeof(tmp)) {
			fprintf(stderr, "Unable to format config option '%s'.\n", confopts[i]);
			goto cleanup;
		}
		if (qrq_config_set_value(&config, &config_len, confopts[i], tmp) != 0) {
			fprintf(stderr, "Unable to update config option '%s'.\n", confopts[i]);
			goto cleanup;
		}
	}

	if (write_file_atomic(rcfilename, config, config_len) != 0) {
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
		
/* Add timestamps to toplist file if not there yet */
static int check_toplist (void) {
	static const char empty_toplist[] = "Toplist   999999 999 1181234567\n";
	char first_line[35] = "";
	char *old_data = NULL;
	char *converted = NULL;
	FILE *fh = NULL;
	size_t file_size;
	size_t first_line_length;
	size_t old_line_length;
	size_t new_line_length;
	size_t converted_size;
	size_t old_offset;
	size_t new_offset;
	long file_length;
	int use_crlf;
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
		if (fclose(fh) != 0) {
			fh = NULL;
			goto cleanup;
		}
		fh = NULL;
		result = write_file_atomic(tlfilename, empty_toplist,
				sizeof(empty_toplist) - 1);
		goto cleanup;
	}
	first_line_length = strlen(first_line);
	use_crlf = first_line_length >= 2 &&
			first_line[first_line_length - 2] == '\r' &&
			first_line[first_line_length - 1] == '\n';
	if ((first_line_length == 32 && !use_crlf) ||
			(first_line_length == 33 && use_crlf)) {
		new_line_length = first_line_length;
		if (fseek(fh, 0, SEEK_END) != 0 || (file_length = ftell(fh)) < 0 ||
				(size_t)file_length % new_line_length != 0) {
			fprintf(stderr, "Invalid toplist format in %s!\n", tlfilename);
			goto cleanup;
		}
		result = 0;
		goto cleanup;
	}
	if ((first_line_length != 21 || use_crlf) &&
			(first_line_length != 22 || !use_crlf)) {
		fprintf(stderr, "Invalid toplist format in %s!\n", tlfilename);
		goto cleanup;
	}
	old_line_length = first_line_length;
	new_line_length = use_crlf ? 33 : 32;

	if (fseek(fh, 0, SEEK_END) != 0 || (file_length = ftell(fh)) < 0) {
		fprintf(stderr, "Unable to determine size of toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	file_size = (size_t)file_length;
	if (file_size % old_line_length != 0 ||
			file_size / old_line_length > SIZE_MAX / new_line_length) {
		fprintf(stderr, "Invalid old-format toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	converted_size = file_size / old_line_length * new_line_length;
	if (fseek(fh, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Unable to rewind toplist file %s!\n", tlfilename);
		goto cleanup;
	}
	old_data = malloc(file_size == 0 ? 1 : file_size);
	converted = malloc(converted_size == 0 ? 1 : converted_size);
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
			old_offset += old_line_length, new_offset += new_line_length) {
		if (old_data[old_offset + 20] != (use_crlf ? '\r' : '\n') ||
				(use_crlf && old_data[old_offset + 21] != '\n')) {
			fprintf(stderr, "Invalid old-format toplist file %s!\n", tlfilename);
			goto cleanup;
		}
		memcpy(converted + new_offset, old_data + old_offset, 20);
		converted[new_offset + 20] = ' ';
		memcpy(converted + new_offset + 21, "1181234567", 10);
		if (use_crlf) {
			converted[new_offset + 31] = '\r';
			converted[new_offset + 32] = '\n';
		} else {
			converted[new_offset + 31] = '\n';
		}
	}

	if (write_file_atomic(tlfilename, converted, converted_size) != 0) {
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
	int destination_opened = 0;
	int saved_errno = 0;
	int result = -1;

	source = fopen(source_path, "rb");
	if (source == NULL) {
		return -1;
	}
	destination = fopen(destination_path, "wb");
	if (destination == NULL) {
		goto cleanup;
	}
	destination_opened = 1;
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) != 0) {
		if (fwrite(buffer, 1, bytes_read, destination) != bytes_read) {
			saved_errno = errno != 0 ? errno : EIO;
			goto cleanup;
		}
	}
	if (ferror(source)) {
		saved_errno = errno != 0 ? errno : EIO;
		goto cleanup;
	}
	if (fclose(destination) != 0) {
		saved_errno = errno;
		destination = NULL;
		goto cleanup;
	}
	destination = NULL;
	result = 0;

cleanup:
	if (destination != NULL) {
		if (fclose(destination) != 0 && saved_errno == 0) {
			saved_errno = errno;
		}
	}
	if (source != NULL) {
		if (fclose(source) != 0 && result == 0) {
			result = -1;
			saved_errno = errno;
		}
	}
	if (result != 0 && destination_opened) {
		(void)unlink(destination_path);
	}
	if (saved_errno != 0) {
		errno = saved_errno;
	}
	return result;
}

#ifdef OSX_BUNDLE
static int set_bundle_resource_directory(const char *program_path) {
	static const char suffix[] = "/Resources";
	char executable_directory[PATH_MAX];
	const char *separator;
	size_t directory_length;
	size_t contents_length;

	if (program_path == NULL || (separator = strrchr(program_path, '/')) == NULL) {
		errno = EINVAL;
		return -1;
	}
	directory_length = (size_t)(separator - program_path);
	if (directory_length == 0 || directory_length >= sizeof(executable_directory)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	memcpy(executable_directory, program_path, directory_length);
	executable_directory[directory_length] = '\0';
	separator = strrchr(executable_directory, '/');
	if (separator == NULL) {
		errno = EINVAL;
		return -1;
	}
	contents_length = (size_t)(separator - executable_directory);
	if (contents_length > sizeof(destdir) - sizeof(suffix)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	memcpy(destdir, executable_directory, contents_length);
	memcpy(destdir + contents_length, suffix, sizeof(suffix));
	return 0;
}
#endif

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
 *    PREFIX/share/qrq/
 * 3) in PREFIX/share/qrq/ -> create ~/.qrq/ and copy qrqrc and toplist
 *    there.
 * 4) Nowhere --> Exit.*/
static int build_path(char *destination, size_t capacity, const char *base,
		const char *suffix) {
	int written;

	if (destination == NULL || capacity == 0 || base == NULL || suffix == NULL) {
		errno = EINVAL;
		return -1;
	}
	written = snprintf(destination, capacity, "%s%s", base, suffix);
	if (written < 0 || (size_t)written >= capacity) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

static int file_is_readable(const char *path) {
	FILE *file = fopen(path, "rb");
	int result;

	if (file == NULL) {
		return 0;
	}
	result = fclose(file) == 0;
	return result;
}

static int create_directory_if_needed(const char *path) {
	struct stat attributes;
	int result;

#ifdef WIN32
	result = mkdir(path);
#else
	result = mkdir(path, 0700);
#endif
	if (result == 0) {
		return 0;
	}
	if (errno != EEXIST || stat(path, &attributes) != 0 ||
			!S_ISDIR(attributes.st_mode)) {
		return -1;
	}
	return 0;
}

static int find_files (void) {
	const char *homedir;
	char config_directory[PATH_MAX];
	char tmp_rcfilename[PATH_MAX] = "";
	char tmp_tlfilename[PATH_MAX] = "";
	char tmp_cbfilename[PATH_MAX] = "";
	int have_current_files;

	printw("\nChecking for necessary files (qrqrc, toplist, callbase)...\n");
	have_current_files = file_is_readable("qrqrc") &&
			file_is_readable("toplist") && file_is_readable("callbase.qcb");
	if (have_current_files) {
		printw("... found in current directory.\n");
		(void)config_string_value(0, "qrqrc path", "qrqrc", rcfilename,
				sizeof(rcfilename), 0);
		(void)config_string_value(0, "toplist path", "toplist", tlfilename,
				sizeof(tlfilename), 0);
		(void)config_string_value(0, "callbase path", "callbase.qcb", cbfilename,
				sizeof(cbfilename), 0);
		(void)config_string_value(0, "summary path", "Summary", sumfilepath,
				sizeof(sumfilepath), 0);
	} else {
		homedir = getenv("HOME");
#ifdef WIN32
		if (homedir == NULL || *homedir == '\0') {
			homedir = getenv("APPDATA");
		}
#endif
		if (homedir == NULL || *homedir == '\0') {
			homedir = ".";
		}
		if (build_path(config_directory, sizeof(config_directory), homedir,
				"/.qrq") != 0 ||
				build_path(rcfilename, sizeof(rcfilename), config_directory,
				"/qrqrc") != 0 ||
				build_path(tlfilename, sizeof(tlfilename), config_directory,
				"/toplist") != 0 ||
				build_path(sumfilepath, sizeof(sumfilepath), config_directory,
				"/Summary") != 0 ||
				build_path(tmp_rcfilename, sizeof(tmp_rcfilename), destdir,
				"/share/qrq/qrqrc") != 0 ||
				build_path(tmp_tlfilename, sizeof(tmp_tlfilename), destdir,
				"/share/qrq/toplist") != 0 ||
				build_path(tmp_cbfilename, sizeof(tmp_cbfilename), destdir,
				"/share/qrq/callbase.qcb") != 0) {
			endwin();
			fprintf(stderr, "Resource path is too long.\n");
			exit(EXIT_FAILURE);
		}
		printw("... not found in current directory. Checking %s...\n",
				config_directory);
		if (!file_is_readable(tmp_cbfilename) ||
				(!file_is_readable(rcfilename) && !file_is_readable(tmp_rcfilename)) ||
				(!file_is_readable(tlfilename) && !file_is_readable(tmp_tlfilename))) {
			endwin();
			fprintf(stderr, "Could not find readable qrqrc, toplist, and "
					"callbase.qcb resources.\n");
			exit(EXIT_FAILURE);
		}
		if (create_directory_if_needed(config_directory) != 0) {
			endwin();
			fprintf(stderr, "Unable to create configuration directory %s: %s\n",
					config_directory, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (!file_is_readable(rcfilename) && copy_file(tmp_rcfilename, rcfilename) != 0) {
			endwin();
			fprintf(stderr, "Unable to copy default config to %s: %s\n",
					rcfilename, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (!file_is_readable(tlfilename) && copy_file(tmp_tlfilename, tlfilename) != 0) {
			endwin();
			fprintf(stderr, "Unable to copy default toplist to %s: %s\n",
					tlfilename, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (config_string_value(0, "callbase path", tmp_cbfilename, cbfilename,
				sizeof(cbfilename), 0) != 0) {
			endwin();
			exit(EXIT_FAILURE);
		}
		printw("... using configuration files in %s.\n", config_directory);
	}
	if (create_directory_if_needed(sumfilepath) != 0) {
		endwin();
		fprintf(stderr, "Unable to create summary directory %s: %s\n",
				sumfilepath, strerror(errno));
		exit(EXIT_FAILURE);
	}
	refresh();
	return 0;
}


static int statistics (void) {
		struct qrq_history_summary summary;
		struct qrq_confusion_summary confusion_summary;
		struct qrq_item_history_summary item_summary;
		int have_confusions;
		int have_item_history;

		if (qrq_history_summarize(historyfilename, mycall, &summary) == 0 &&
				summary.sessions != 0) {
			have_confusions = qrq_confusion_summarize(confusionfilename, mycall,
					&confusion_summary) == 0 && confusion_summary.pair_count != 0;
			have_item_history = qrq_item_history_summarize(itemhistoryfilename, mycall,
					&item_summary) == 0 && item_summary.attempts != 0;
			werase(mid_w);
			box(mid_w, 0, 0);
			wattron(mid_w, A_BOLD);
			mvwprintw(mid_w, 1, 2, "Session history for %s", mycall);
			wattroff(mid_w, A_BOLD);
			mvwprintw(mid_w, 3, 2, "Sessions:          %zu", summary.sessions);
			mvwprintw(mid_w, 4, 2, "Average score:     %d", summary.average_score);
			mvwprintw(mid_w, 5, 2, "Average accuracy:  %d%%", summary.average_accuracy);
			mvwprintw(mid_w, 6, 2, "Best score/speed:  %d / %d CpM",
					summary.best_score, summary.best_speed);
			mvwprintw(mid_w, 7, 2, "Score trend:       %d -> %d",
					summary.first_score, summary.last_score);
			if (have_item_history) {
				unsigned int accuracy = (unsigned int)(100.0 *
						(double)item_summary.correct / (double)item_summary.attempts);
				unsigned long long average_ms = (unsigned long long)
						(item_summary.total_response_ms / item_summary.attempts);

				mvwprintw(mid_w, 8, 2, "Items: %u%% correct, %llums average answer time",
						accuracy, average_ms);
			}
			if (have_confusions) {
				size_t index;

				mvwprintw(mid_w, 9, 2, "Frequent copy differences (%zu total):",
						confusion_summary.errors);
				for (index = 0; index < confusion_summary.pair_count && index < 3;
						index++) {
					char expected[10];
					char received[10];
					const struct qrq_confusion_pair *pair =
							&confusion_summary.pairs[index];

					if (pair->expected == '\0') {
						(void)snprintf(expected, sizeof(expected), "extra");
					} else if (pair->expected == ' ') {
						(void)snprintf(expected, sizeof(expected), "space");
					} else {
						(void)snprintf(expected, sizeof(expected), "%c", pair->expected);
					}
					if (pair->received == '\0') {
						(void)snprintf(received, sizeof(received), "omitted");
					} else if (pair->received == ' ') {
						(void)snprintf(received, sizeof(received), "space");
					} else {
						(void)snprintf(received, sizeof(received), "%c", pair->received);
					}
					mvwprintw(mid_w, 10 + (int)index, 2, "%-8s -> %-8s %zu",
							expected, received, pair->count);
				}
			} else {
				mvwaddstr(mid_w, 10, 2, "No character-level copy differences recorded yet.");
			}
			if (have_item_history && item_summary.difficult_count != 0) {
				const struct qrq_item_history_item *item = &item_summary.difficult[0];

				mvwprintw(mid_w, 13, 2, "Most missed: %-28.28s (%zu/%zu errors)", item->sent,
						item->errors, item->attempts);
			}
			mvwaddstr(mid_w, 15, 2, "Press any key to return.");
			wrefresh(mid_w);
			getch();
			touchwin(mid_w);
			return 0;
		}

		char line[80]="";

		long timestamp = 0;
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
				if (qrq_toplist_callsign_matches(line, mycall)) {
					if (sscanf(line + 10, "%6d %*3d %10ld", &score,
							&timestamp) == 2) {
						count++;
						fprintf(fh2, "%ld %d\n", timestamp, score);
					}
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
	free(call_used);
	call_used = NULL;
	free(call_mistakes);
	call_mistakes = NULL;
	free(call_spaced_due);
	call_spaced_due = NULL;
	spaced_due_count = 0;
	qrq_review_queue_clear(&review_queue);
	qrq_callbase_free(&loaded_callbase);
	calls = loaded_callbase.items;
	calls_allocated = 0;
}

static void apply_confusion_focus(void) {
	char symbols[QRQ_CONFUSION_TOP_COUNT * 2 + 1];
	int retain_result;

	focusconfusions_active = 0;
	if (!focusconfusions ||
			qrq_confusion_focus_symbols(confusionfilename, mycall, symbols,
					sizeof(symbols)) != 0 || symbols[0] == '\0') {
		return;
	}
	retain_result = qrq_callbase_retain_symbols(&loaded_callbase, symbols);
	if (retain_result == 0) {
		focusconfusions_active = 1;
	}
}

static size_t read_callbase(void) {
	const struct qrq_callbase_filter filter = {
		.minimum_length = (size_t)mincalllength,
		.maximum_length = (size_t)maxcalllength,
		.prefixes = callprefixes,
		.digit_mode = digitmode,
		.portable_mode = portablevariants ? 2 : portablemode,
		.allowed_chars = allowedchars,
	};

	free_calls();
	if (serialdigits != 0) {
		if (qrq_callbase_generate_serials((unsigned int)serialdigits,
					&loaded_callbase) != 0) {
			endwin();
			fprintf(stderr, "Error: Couldn't generate serial exchanges: %s\n",
					strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else if (qrq_callbase_load(cbfilename, &filter, &loaded_callbase) != 0) {
		endwin();
		fprintf(stderr, "Error: Couldn't load callsign database '%s': %s\n",
				cbfilename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (serialdigits == 0 && portablevariants &&
			qrq_callbase_generate_portable_variants(&loaded_callbase) != 0) {
		endwin();
		fprintf(stderr, "Error: Couldn't generate portable calls: %s\n",
				strerror(errno));
		exit(EXIT_FAILURE);
	}
	apply_confusion_focus();
	calls = loaded_callbase.items;
	calls_allocated = loaded_callbase.count;
	call_used = calloc(calls_allocated, sizeof(*call_used));
	call_mistakes = calloc(calls_allocated, sizeof(*call_mistakes));
	if (spacedrepetition) {
		call_spaced_due = calloc(calls_allocated, sizeof(*call_spaced_due));
	}
	if (call_used == NULL || call_mistakes == NULL ||
			(spacedrepetition && call_spaced_due == NULL)) {
		free_calls();
		endwin();
		fprintf(stderr, "Error: Couldn't allocate call usage state.\n");
		exit(EXIT_FAILURE);
	}
	if (spacedrepetition && qrq_item_history_schedule(itemhistoryfilename, mycall,
				(const char *const *)calls, calls_allocated, call_spaced_due) == 0) {
		size_t index;

		for (index = 0; index < calls_allocated; ++index) {
			if (call_spaced_due[index] != 0) {
				spaced_due_count++;
			}
		}
	}
	call_maxlen = (int)loaded_callbase.max_length;
	return loaded_callbase.count;
}

static int has_qcb_suffix(const char *name) {
	size_t length = strlen(name);

	return length > 4 && name[length - 4] == '.' &&
			tolower((unsigned char)name[length - 3]) == 'q' &&
			tolower((unsigned char)name[length - 2]) == 'c' &&
			tolower((unsigned char)name[length - 1]) == 'b';
}

static int callbase_is_listed(const char *path) {
	size_t index;

	for (index = 0; index < cblist_count; index++) {
		if (strcmp(cblist[index], path) == 0) {
			return 1;
		}
	}
	return 0;
}

void find_callbases (void) {
	DIR *directory;
	struct dirent *entry;
	char candidate[PATH_MAX];
	char path[3][PATH_MAX] = {{0}};
	const char *homedir = getenv("HOME");
	size_t path_count = 0;
	size_t path_index;

	cblist_count = 0;
	memset(cblist, 0, sizeof(cblist));
	if (getcwd(path[path_count], sizeof(path[path_count])) != NULL) {
		path_count++;
	}
#ifdef WIN32
	if (homedir == NULL || *homedir == '\0') {
		homedir = getenv("APPDATA");
	}
#endif
	if (homedir != NULL && *homedir != '\0' && path_count < 3 &&
			build_path(path[path_count], sizeof(path[path_count]), homedir,
			"/.qrq") == 0) {
		path_count++;
	}
	if (path_count < 3 && build_path(path[path_count], sizeof(path[path_count]),
			destdir, "/share/qrq") == 0) {
		path_count++;
	}

	for (path_index = 0; path_index < path_count && cblist_count < 100;
			path_index++) {
		directory = opendir(path[path_index]);
		if (directory == NULL) {
			continue;
		}
		while (cblist_count < 100 && (entry = readdir(directory)) != NULL) {
			int written;

			if (!has_qcb_suffix(entry->d_name)) {
				continue;
			}
			written = snprintf(candidate, sizeof(candidate), "%s/%s",
					path[path_index], entry->d_name);
			if (written < 0 || (size_t)written >= sizeof(candidate) ||
					callbase_is_listed(candidate)) {
				continue;
			}
			memcpy(cblist[cblist_count], candidate, (size_t)written + 1);
			cblist_count++;
		}
		(void)closedir(directory);
	}
}



void select_callbase (void) {
	int i = (int)cblist_count, j = 0, k = 0;
	int c = 0;		/* cursor position   */
	int p = 0;		/* page a 10 entries */
	char* cblist_ptr;


	curs_set(FALSE);

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
		if (j < i) {
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
			(void)snprintf(cbfilename, sizeof(cbfilename), "%s", cblist[c]);
			return;	
		case 27:
		case 'q':
		case 'Q':
			return;
	}

	wrefresh(conf_w);

	} /* while 1 */

}

int validchar (int c) {
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0'  && c <= '9') || c == '/' || c == ' ' || c == '-' || c == '.' || c == ',' || c == '=' || c == '?');
}

static void print_version(void) {
	printf("qrq %s\n", VERSION);
}

static void help(void) {
		printf("qrq v%s  (c) 2006-2025 Fabian Kurz, DJ5CW. "
					"http://fkurz.net/ham/qrq.html\n", VERSION);
		printf("High speed morse telegraphy trainer, similar to"
					" RUFZ.\n\n");
		printf("This is free software, and you are welcome to" 
						" redistribute it\n");
		printf("under certain conditions (see COPYING).\n\n");
		printf("Start 'qrq' without any command line arguments for normal"
					" operation.\n\n");
		printf("Use 'qrq --version' to print the version number.\n\n");
#ifdef BUILD_INFO
        printf("Build info for this executable:\n%s\n", BUILD_INFO);
#endif
		exit(0);
}


/* vim: noai:ts=4:sw=4 
*/
