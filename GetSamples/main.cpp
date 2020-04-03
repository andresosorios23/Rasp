#include <wiringPiI2C.h>
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
#include "ADS1115.h"


#define SIGNAL_uS 1700 //sampling period in uS
#define SAMPLES 1000 
#define SHM_KEY 0x1234
#define V_CHANNEL 0
#define I_CHANNEL 3

struct windowseg {
	time_t timestamp;
	int busy;
	double voltage[SAMPLES];
	double current[SAMPLES];
};

struct tm* ts;
struct windowseg* window;
int shmid, cnt;
const size_t mmove_size = (SAMPLES - 1) * sizeof(double);
ADS1115 ads;
uint16_t adc_voltage, adc_current;

void alarm_callback(int signum) {
	if (cnt == 10) {
		window->busy = 1;
		time(&window->timestamp);
		ts = localtime(&window->timestamp);
		memmove(&window->voltage[0], &window->voltage[1], mmove_size);
		memmove(&window->current[0], &window->current[1], mmove_size);
		adc_voltage = ads.readADC_SingleEnded(V_CHANNEL);
		adc_current = ads.readADC_SingleEnded(I_CHANNEL);
		window->voltage[SAMPLES - 1] = adc_voltage * 0.1252 / 1000;
		window->current[SAMPLES - 1] = adc_current * 0.1252 / 1000;
		window->busy = 0;
		//printf("timestamp: %s, v: %.3f, c: %.3f\n", asctime(ts), window->voltage[SAMPLES - 1], window->current[SAMPLES - 1]);
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

void* thread_func(void* data)
{
	ads = ADS1115();
	ads.setGain(GAIN_ONE);
	ads.begin();
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

int main(void)
{
	struct sched_param param;
	pthread_attr_t attr;
	pthread_t thread;
	int ret;

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
		printf("mlockall failed: %m\n");
		exit(-2);
	}
	ret = pthread_attr_init(&attr);
	if (ret) {
		printf("init pthread attributes failed\n");
		goto out;
	}
	ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
	if (ret) {
		printf("pthread setstacksize failed\n");
		goto out;
	}
	ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	if (ret) {
		printf("pthread setschedpolicy failed\n");
		goto out;
	}
	param.sched_priority = 80;
	ret = pthread_attr_setschedparam(&attr, &param);
	if (ret) {
		printf("pthread setschedparam failed\n");
		goto out;
	}
	ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if (ret) {
		printf("pthread setinheritsched failed\n");
		goto out;
	}
	ret = pthread_create(&thread, &attr, thread_func, NULL);
	if (ret) {
		printf("create pthread failed\n");
		goto out;
	}
	ret = pthread_join(thread, NULL);
	if (ret)
		printf("join pthread failed: %m\n");

out:
	return ret;
}