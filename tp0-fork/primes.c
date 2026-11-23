#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void
filtro(int fd_lectura);  // para que el compilador sepa que existe esta funcion antes


void
generador(int n)
{
	int pipe_generador_filtro[2];
	int codigo_estado_generador_filtro = pipe(pipe_generador_filtro);

	if (codigo_estado_generador_filtro < 0) {
		fprintf(stderr, "Error: el pipe no funciona");
		exit(1);
	}

	int pid = fork();  // se llama pid porque eso es lo que le devuelve al padre, el pid de su hijo

	if (pid > 0) {  // verifica si es proceso padre
		close(pipe_generador_filtro[0]);  // no necesita leer

		for (int numero = 2; numero <= n; numero++) {
			int bytes_escritos = write(pipe_generador_filtro[1],
			                           &numero,
			                           sizeof(numero));
			if (bytes_escritos < 0) {
				fprintf(stderr,
				        "Error: fallo el write del generador");
				exit(1);
			}
		}

		close(pipe_generador_filtro[1]);  // no necesita escribir mas
		wait(NULL);

	} else if (pid == 0) {                    // verifica si es proceso hijo
		close(pipe_generador_filtro[1]);  // no necesita escribir
		filtro(pipe_generador_filtro[0]);  // empieza su trabajo de filtrar.
		                                   // le llega por el extremo de
		                                   // lectura los numeros que el
		                                   // generador (proceso padre) esta escribiendo

	} else {
		fprintf(stderr, "Error al crear el proceso hijo");
		exit(1);
	}
}


// funcion recursiva que se queda con un numero primo y descarta sus multiplos
// fd_lectura: extremo de lectura del pipe con el anterior pipe (puede venir de
// la funcion generador, si es el primer filtro, o de otro filtro anterior )
void
filtro(int fd_lectura)
{
	int primo;
	int bytes_leidos =
	        read(fd_lectura,
	             &primo,
	             sizeof(primo));  // lee el primer numero que le llega

	if (bytes_leidos < 0) {
		fprintf(stderr, "Error: fallo el read del primer numero");
		exit(1);
	}

	if (bytes_leidos == 0) {  // caso base
		exit(0);
	}

	int pipe_filtro_hijo[2];
	int codigo_estado_filtro_hijo =
	        pipe(pipe_filtro_hijo);  // creo el pipe hacia el proximo filtro

	if (codigo_estado_filtro_hijo < 0) {
		fprintf(stderr, "Error: el pipe no funciona");
		exit(1);
	}

	int pid = fork();  // creo el proximo filtro

	if (pid > 0) {
		printf("primo %d\n",
		       primo);  // se imprime aca, despues del fork,
		                // para que no se duplique el buffer entre padre e hijo

		close(pipe_filtro_hijo[0]);

		int numero;

		while ((bytes_leidos = read(fd_lectura, &numero, sizeof(numero))) >
		       0) {  // lee los numeros siguientes de a uno hasta que se
			     // terminen (o sea hasta que se cierre la lectura)
			if (numero % primo != 0) {
				int bytes_escritos = write(pipe_filtro_hijo[1],
				                           &numero,
				                           sizeof(numero));  // si no es multiplo del primo lo reenvio al proximo filtro
				if (bytes_escritos < 0) {
					fprintf(stderr, "Error: fallo el write del filtro");
					exit(1);
				}
			}
		}

		if (bytes_leidos < 0) {
			fprintf(stderr, "Error: fallo el read del filtro");
			exit(1);
		}

		close(pipe_filtro_hijo[1]);
		close(fd_lectura);  // ya termine de usar este fd, lo cierro
		wait(NULL);         // espera toda la subcadena de hijos

	} else if (pid == 0) {  // se convierte en el proximo filtro de la cadena
		close(pipe_filtro_hijo[1]);
		close(fd_lectura);
		filtro(pipe_filtro_hijo[0]);  // llamada recursiva

	} else {
		fprintf(stderr, "Error al crear el proceso hijo");
		exit(1);
	}
}


int
main(int argc, char *argv[])
{  // argc es un entero que dice cuantas palabras separadas por espacio escribi
   // en la terminal. argv es el array de strings en donde cada posicion tiene una de esas palabras

	if (argc == 2) {  // en la consola ./primes cuenta como primer palabra
		int n = atoi(
		        argv[1]);  // convierto string que le paso en consola en entero

		if (n < 2) {
			fprintf(stderr, "Error: le pasaste un numero invalido");
			return 1;
		}
		generador(n);  // con todo verificado correctamente se pueden
		               // generar los numeros que van a ser filtrados

	} else {
		fprintf(stderr, "Error: no le pasaste el numero");
		return 1;
	}
	return 0;
}