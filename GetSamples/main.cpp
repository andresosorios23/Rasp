#include <stdio.h> 
#include <stdlib.h>
#include <signal.h>
#include <unistd.h> 
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <ctime>

#define SIGNAL_uS 500 //Periodo de muestreo, en microsegundos
#define SAMPLES 1000 //Tamaño del vector de muestras
#define SHM_KEY 0x1234 //Direccion de la zona de memoria compartida

//definicion de la estructura para almacenar los datos

struct windowseg {
	int busy;
	time_t timestamp;
	double voltage[SAMPLES];
	double current[SAMPLES];
};

struct tm* ts;
struct windowseg* window;
int shmid, cnt;
float f, phase, I, V;
const size_t mmove_size = (SAMPLES - 1) * sizeof(double);

double adc_voltage, adc_current, t;


//Función para almacenar los datos en la zona de memoria compartida
void alarm_callback(int signum) {
	if (cnt == 10) {
		window->busy = 1;
		time(&window->timestamp);
		t = t + float(SIGNAL_uS) / 1000000.0; //contador de tiempo
		memmove(&window->voltage[0], &window->voltage[1], mmove_size); //Se mueve la ventana de tiempo para poder almacenar un nuevo dato
		memmove(&window->current[0], &window->current[1], mmove_size);
		adc_voltage = V * M_SQRT2 * cos(f * 2 * M_PI * t);
		adc_current = I * M_SQRT2 * cos(f * 2 * M_PI * t + phase*M_PI/180);
		window->voltage[SAMPLES - 1] = adc_voltage;
		window->current[SAMPLES - 1] = adc_current;
		window->busy = 0;
		cnt = -1;
	}
	cnt++;
}
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
{	//Inicializacion de la memoria compartida en modo de escritura
	shmid = shmget(SHM_KEY, sizeof(struct windowseg), 0644 | IPC_CREAT);
	if (shmid == -1) {
		printf("shared memory segment failed\n");
	}
	window = (windowseg*)shmat(shmid, NULL, 0);
	if (window == (void*)-1) {
		printf("shared memory attach failed\n");
	}
	printf("initialization succeed and shmem (w) interface opened\n");
	printf("Ingrese la frecuencia de las senales en Hz: ");
	scanf("%f", &f);
	printf("Ingrese la magnitud RMS de la corriente en amperios: ");
	scanf("%f", &I);
	printf("Ingrese la magnitud RMS del voltaje en voltios: ");
	scanf("%f", &V);
	printf("Ingrese la fase de la corriente en grados: ");
	scanf("%f", &phase);
	printf("Simulando...");



	//Configuración de las alarmas para ejecutar o terminar funciones
	signal(SIGALRM, alarm_callback); //ejecutar la simulacion de datos cada cierto tiempo
	signal(SIGINT, term_callback); // interrumpir la interfaz de memoria compartida con el teclado si se desea

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
