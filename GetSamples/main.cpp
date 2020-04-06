#include <arpa/inet.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h> 
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string>
#include <cstdio>
#include <ctime>

#define SIGNAL_uS 500 //sampling period in uS
#define SAMPLES 1000 
#define SHM_KEY 0x1234


struct windowseg {
	int busy;
	time_t timestamp;
	double voltage[SAMPLES];
	double current[SAMPLES];
};

struct tm* ts;
struct windowseg* window;
int shmid, cnt;
const size_t mmove_size = (SAMPLES - 1) * sizeof(double);

double adc_voltage, adc_current, t;
int f;


void alarm_callback(int signum) {
	if (cnt == 10) {
		window->busy = 1;
		time(&window->timestamp);
		t = t + float(SIGNAL_uS) / 1000000.0;
		memmove(&window->voltage[0], &window->voltage[1], mmove_size);
		memmove(&window->current[0], &window->current[1], mmove_size);
		adc_voltage = 20 * cos(58 * 2 * M_PI * t);
		adc_current = 20 * cos(58 * 2 * M_PI * t);
		window->voltage[SAMPLES - 1] = adc_voltage;
		window->current[SAMPLES - 1] = adc_current;
		window->busy = 0;
		//printf("%f\n",window->voltage[SAMPLES - 1]);
		cnt = -1;
	}
	cnt++;
}

void term_callback(int signum) {
	if (shmdt(window) == -1) {
		printf("shared memory detaching failed\n");
	}
	if (shmctl(shmid, IPC_RMID, 0) == -1) {
		printf("shmctl failed\n");
	}
	printf("shmem (w) interface closed... goodbye\n");
}

int main(void)
{	
	shmid = shmget(SHM_KEY, sizeof(struct windowseg), 0644 | IPC_CREAT);
	if (shmid == -1) {
		printf("shared memory segment failed\n");
	}
	window = (windowseg*)shmat(shmid, NULL, 0);
	if (window == (void*)-1) {
		printf("shared memory attach failed\n");
	}
	printf("initialization succeed and shmem (w) interface opened\n");


	signal(SIGALRM, alarm_callback);
	signal(SIGINT, term_callback);

	while (1) {
		ualarm(SIGNAL_uS, SIGNAL_uS);
		sleep(1000);
	}
	if (shmdt(window) == -1) {
		printf("shared memory detaching failed\n");
	}
	if (shmctl(shmid, IPC_RMID, 0) == -1) {
		printf("shmctl failed\n");
	}
	printf("shmem (w) interface closed... goodbye\n");
}
