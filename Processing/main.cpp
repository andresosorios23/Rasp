#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <string>
#include <ctime>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <complex.h>
#include "./alglib/fasttransforms.h"

#define SIGNAL_uS 500 //Periodo de muestreo, en microsegundos
#define SAMPLES 1000 //Tamaño del vector de muestras
#define SHM_KEY 0x1234 //Direccion de la zona de memoria compartida
#define PORT 20001 //Puerto del servidor


// declaración de las variables a usar
int sockfd;
double res;
char* hello = "1";
struct sockaddr_in servaddr;
int n;
int shmid;
double f_v, f_c, mag_v, mag_c, theta_v, theta_c, fp;
double fs = 1000000.0 / float(SIGNAL_uS); //Frecuencia de muestreo
const double f_step = fs / float(SAMPLES); //resolución en frecuencia
socklen_t len;

//Estructura donde se guardan los datos
struct windowseg {
	int busy;
	time_t timestamp;
	double voltage[SAMPLES];
	double current[SAMPLES];
};

struct windowseg* window;

// función para finalizar la interfaz de memoria compartida
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
{ //Inicialización del socket de cliente
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	servaddr.sin_addr.s_addr = inet_addr("192.168.1.2");
	sendto(sockfd, (const char*)hello, strlen(hello), MSG_CONFIRM, (const struct sockaddr*) & servaddr, sizeof(servaddr));
	printf("Hello message sent.\n");

	//Arreglos para procesamiento
	alglib::real_1d_array voltage_array;
	alglib::real_1d_array current_array;
	alglib::complex_1d_array sp_v;
	alglib::complex_1d_array sp_c;

	
	printf("%f", f_step);

	// Inicialización de lectura de zona de memoria compartida
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

	// Inicio del procesamiento
	while (1) {
		while (window->busy != 1) {

			mag_v, mag_c, theta_v, theta_c, f_c, fp = 0.0;
			//Asignación del contenido de la seccion de memoria compartida a los vectores
			voltage_array.setcontent(SAMPLES, window->voltage);
			current_array.setcontent(SAMPLES, window->current);

			//Aplicacion de la fft a los vectores y selección del armónico fundamental
			alglib::fftr1d(voltage_array, sp_v, alglib::xdefault);
			for (int i = 0; i < sp_v.length() / 2; i++) {
				if (alglib::abscomplex(sp_v[i]) > mag_v) {
					mag_v = 2*alglib::abscomplex(sp_v[i]) / ((SAMPLES)*M_SQRT2);
					theta_v = atan(sp_v[i].y / sp_v[i].x);
					f_v = i * f_step;
				}
			}
			
			alglib::fftr1d(current_array, sp_c, alglib::xdefault);
			for (int i = 0; i < sp_c.length() / 2; i++) {
				if (alglib::abscomplex(sp_c[i]) > mag_c) {
					mag_c = 2*alglib::abscomplex(sp_c[i]) / ((SAMPLES)*M_SQRT2);
					theta_c = atan(sp_c[i].y / sp_c[i].x);
					f_c = i * f_step;
				}
			}
			//Se fija la referencia a la fase del voltaje
			theta_c -=theta_v;
			if (theta_c < -M_PI / 2.0) {
				theta_c += M_PI;
			}
			theta_v = 0;

			//Cálculo del factor de potencia
			fp = abs(cos(theta_v - theta_c));

			//Conversión del ángulo a grados para su fácil visualización
			double theta_vd = theta_v * 180.0 / M_PI;
			double theta_cd = theta_c * 180.0 / M_PI;
			float p = mag_c * mag_v * fp;

			//Envio de los datos al servidor
			char timestamp[40];
			char value[100];
			sprintf(timestamp,"timestamp: %s", asctime(localtime(&window->timestamp)));
			sendto(sockfd, (const char*)timestamp, strlen(timestamp), MSG_CONFIRM, (const struct sockaddr*) & servaddr, sizeof(servaddr));
			sprintf(value,"V = %.4f /_ %.2f[V]; I = %.4f /_ %.2f [A]; F = %.1f [Hz]; P = %.1f [W]  FP = %.2f",mag_v, theta_vd, mag_c, theta_cd, f_v, p, fp);
			sendto(sockfd, (const char*)value, strlen(value), MSG_CONFIRM, (const struct sockaddr*) & servaddr, sizeof(servaddr));

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