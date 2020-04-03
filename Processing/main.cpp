#include <wiringPi.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stdlib.h>
#include <complex.h>
#include "./alglib/fasttransforms.h"


#define SIGNAL_uS 500 //sampling period in uS
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

struct windowseg* window;
int shmid;

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

	alglib::real_1d_array voltage_array;
	alglib::real_1d_array current_array;
	alglib::complex_1d_array sp_v;
	alglib::complex_1d_array sp_c;
	alglib::real_1d_array xcorr;
	double f_v, f_c, mag_v, mag_c, theta_v, theta_c, fp;
	const double f_step = float(SIGNAL_uS) / float(SAMPLES);

	shmid = shmget(SHM_KEY, sizeof(struct windowseg), 0644 | IPC_CREAT);
	if (shmid == -1) {
		printf("shared memory segment failed\n");
		return 1;
	}
	window = (windowseg*)shmat(shmid, NULL, 0);
	if (window == (void*)-1) {
		printf("shared memory attach failed\n");
		return 1;
	}
	printf("shmem (r) interface opened\n");
	signal(SIGINT, term_callback);
	while (1) {
		while (window->busy != 1) {
			mag_c, theta_c, f_c, fp = 0.0;
			voltage_array.setcontent(SAMPLES, window->voltage);
			current_array.setcontent(SAMPLES, window->current);
			alglib::fftr1d(voltage_array, sp_v, alglib::xdefault);
			for (int i = 0; i < sp_v.length() / 2; i++) {
				if (alglib::abscomplex(sp_v[i]) > mag_v) {
					mag_v = alglib::abscomplex(sp_v[i]) / (SAMPLES);
					theta_v = atan(sp_v[i].y / sp_v[i].x) * 180 / alglib::pi();
					f_v = i * f_step;
				}
			}
			alglib::fftr1d(current_array, sp_c, alglib::xdefault);
			for (int i = 0; i < sp_c.length() / 2; i++) {
				if (alglib::abscomplex(sp_c[i]) > mag_c) {
					mag_c = alglib::abscomplex(sp_c[i]) / (SAMPLES);
					theta_c = atan(sp_c[i].y / sp_c[i].x) * 180 / alglib::pi();
					f_c = i * f_step;
				}
			}
			printf("timestamp: %s", asctime(localtime(&window->timestamp)));
			printf("V = %.4f/_%.2f [V]; I = %.4f/_%.2f [A]; F = %.2f [Hz]; FP = %.2f\n",
				mag_v, theta_v, mag_c, theta_c, f_v, fp);
			sleep(1);
		}
		usleep(500);
	}
	if (shmdt(window) == -1) {
		printf("shared memory detaching failed\n");
		return 1;
	}
	printf("shmem (r) interface closed... goodbye\n");
	return 0;
}