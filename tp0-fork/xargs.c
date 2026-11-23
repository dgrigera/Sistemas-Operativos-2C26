#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#ifndef NARGS
#define NARGS 4
#endif

// paquete es un array que va a contener el comando, los argumentos leidos y un
// NULL como lo exige el manual de execvp(3) para poder ejecutarlo despues
int
armar_paquete(char *paquete[NARGS + 2], char *comando)
{  //+2: comando posicion 0 y NULL final
	int vuelta = 0;
	char *linea = NULL;
	size_t tamano_buffer = 0;
	int caracteres_leidos;

	paquete[0] = comando;

	// lee hasta NARGS argumentos cortando antes si se termina la entrada
	while (vuelta < NARGS &&
	       (caracteres_leidos = getline(&linea, &tamano_buffer, stdin)) >= 0) {
		char *salto_linea = strchr(linea, '\n');  // busca el \n si existe
		if (salto_linea != NULL) {
			*salto_linea = '\0';  // lo corta solo si lo encontro
		}
		paquete[vuelta + 1] =
		        linea;  // guarda el puntero a la linea, corrido una posicion porque 0 es el comando

		vuelta++;
		linea = NULL;  // reinicio para que la proxima vuelta getline reserve memoria nueva
		tamano_buffer = 0;
	}
	free(linea);  // por si getline reservo memoria en el intento que fallo
	paquete[vuelta + 1] =
	        NULL;  // marca el final del array de argumentos para execvp
	return vuelta;
}


void
ejecutar_comando(char *paquete[NARGS + 2])
{
	int pid = fork();

	if (pid > 0) {
		wait(NULL);

	} else if (pid == 0) {
		execvp(paquete[0], paquete);  // el hijo se convierte en el comando
		fprintf(stderr, "Error al ejecutar el comando");
		exit(1);

	} else {
		fprintf(stderr, "Error al crear el proceso hijo");
		exit(1);
	}
}


// de cada linea leida libera memoria para evitar mem leaks
void
liberar_paquete(char *paquete[NARGS + 2], int cantidad)
{
	for (int posicion = 1; posicion <= cantidad; posicion++) {
		free(paquete[posicion]);
	}
}


int
main(int argc, char *argv[])
{
	if (argc == 2) {  // en la consola ./xargs cuenta como primer palabra
		char *comando =
		        argv[1];  // el comando que hay que ejecutar por cada paquete
		char *paquete[NARGS + 2];
		int cantidad;

		while ((cantidad = armar_paquete(paquete, comando)) >
		       0) {  // mientras se puedan seguir armando paquetes con al menos un argumento
			ejecutar_comando(
			        paquete);  // crea el hijo que ejecuta el comando y el padre espera
			liberar_paquete(paquete, cantidad);
		}

	} else {
		fprintf(stderr, "Error: no le pasaste el comando");
		return 1;
	}
	return 0;
}