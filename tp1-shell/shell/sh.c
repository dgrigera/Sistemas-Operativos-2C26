#include "defs.h"
#include "types.h"
#include "readline.h"
#include "runcmd.h"

char prompt[PRMTLEN] = { 0 };

// Manejador de SIGCHLD: el kernel nos manda esta senal automaticamente
// cada vez que un hijo nuestro cambia de estado (aca nos importa el
// caso en el que TERMINA).
//
// Un solo disparo del manejador puede tener que dar cuenta de VARIOS
// hijos que hayan terminado, porque las senales de un mismo tipo "se
// pisan" entre si: si mientras el manejador esta corriendo (o mientras
// SIGCHLD esta bloqueada, ver run_cmd() en runcmd.c) terminan dos
// procesos en 2do plano, el kernel no nos deja dos SIGCHLD pendientes,
// sino uno solo. Por eso hacemos un while con WNOHANG: vamos
// preguntando "hay algun hijo mio que ya haya terminado?" hasta que
// la respuesta sea "no hay mas por ahora" (waitpid devuelve 0) o "no
// tengo ningun hijo en ese grupo" (devuelve -1).
//
// El "0" como primer argumento de waitpid() es la clave de todo el
// mecanismo: significa "espera por cualquier hijo cuyo grupo de
// procesos sea igual al MIO" (el de quien llama a waitpid(), o sea la
// propia shell, ya que este manejador corre dentro del proceso de la
// shell). En run_cmd() (runcmd.c) sacamos a todo proceso de PRIMER
// plano de nuestro grupo con setpgid(), y dejamos a los de 2do plano
// sin tocar (siguen en el grupo original de la shell). Entonces aca
// solo "caen" los de 2do plano, que son justo los unicos que nos
// interesa reapear de esta forma asincronica.
static void
manejador_sigchld(int senal)
{
	// no usamos el numero de senal, pero sigaction exige que el
	// manejador tenga esta firma
	(void) senal;

	// waitpid() puede modificar errno. Si justo estabamos en medio
	// de otra syscall en el flujo "normal" del programa cuando nos
	// interrumpio la senal, no queremos pisarle el errno que esa
	// syscall haya dejado.
	int errno_anterior = errno;

	pid_t pid;
	int estado;

	while ((pid = waitpid(0, &estado, WNOHANG)) > 0) {
		// Nota: fprintf() no esta
		// garantizado como "async-signal-safe" por POSIX por las dudas hay que ver
		// man 7 signal-safety, porque por adentro usa buffers
		// y en algunas implementaciones puede llegar a pedir
		// memoria. eso en la practica, con glibc y en un programa
		// de un solo hilo como este, funciona bien, y es lo
		// mismo que ya usa el resto del esqueleto
		// (print_status_info(), print_back_info() en
		// printstatus.c) para imprimir. una mejor version
		// seria armar el mensaje a mano sin printf y lo
		// mandaria con write(2), que si es async-signal-safe.
		//
		// Este mensaje no se encapsula con SHELL_NO_INTERACTIVE:
		// segun aclaro Patricio en la Clase 5, ese macro es
		// solamente para el prompt (readline) y para
		// print_status_info(), no para salidas funcionales de la
		// shell como esta. isatty(1) alcanza para no ensuciar la
		// salida cuando stdout no es una terminal (por ejemplo,
		// corriendo bajo los tests).
		if (isatty(1))
			fprintf(stdout,
			        "%s==> terminado: PID=%d%s\n",
			        COLOR_BLUE,
			        pid,
			        COLOR_RESET);
	}

	errno = errno_anterior;
}

// instala manejador_sigchld() para que la shell se entere de que un
// proceso en segundo plano termino apenas ocurre, sin tener que ir
// preguntando activamente (polling), ni bloquearse esperandolo
static void
init_signals()
{
	// Tamano fijo para la pila alternativa del handler. No usamos
	// el macro SIGSTKSZ como tamano de este array porque en los
	// toolchains nuevos no es una constante de tiempo de
	// compilacion, y no usamos malloc porque Patricio dijo en
	// clase que no hace falta memoria dinamica para esto: se
	// configura una unica vez al inicializar la shell.
	static char pila_handler[8192];
	stack_t pila_alternativa;
	struct sigaction accion;

	// Le damos al handler un pedazo de memoria separado del stack
	// normal del proceso, con sigaltstack(2). La idea, segun lo
	// que explico Patricio en la Clase 4, es que si el codigo del
	// handler llegara a romper algo (un stack overflow, lo que
	// sea) no afecte al stack normal de ejecucion de la shell.
	pila_alternativa.ss_sp = pila_handler;
	pila_alternativa.ss_size = sizeof pila_handler;
	pila_alternativa.ss_flags = 0;

	if (sigaltstack(&pila_alternativa, NULL) < 0) {
		perror("sigaltstack");
		exit(1);
	}

	memset(&accion, 0, sizeof accion);
	accion.sa_handler = manejador_sigchld;

	// Si una syscall "lenta" por ejemplo el read() que hace
	// read_line() para leer el proximo comando desde la terminal;
	// esta bloqueada justo cuando llega un SIGCHLD, sin este flag
	// esa syscall devolveria -1 con errno == EINTR. Con SA_RESTART,
	// el kernel la reintenta solo apenas termina el handler, asi
	// que la shell no se rompe ni tenemos que andar revisando EINTR
	// por todos lados.
	//
	// SA_ONSTACK le dice al kernel que, cuando dispare este
	// handler, lo corra sobre la pila alternativa que acabamos de
	// configurar con sigaltstack(), en vez de sobre el stack normal
	// del proceso.
	accion.sa_flags = SA_RESTART | SA_ONSTACK;

	// mientras se esta ejecutando el handler, bloqueamos mas
	// SIGCHLD para que no se interrumpa a si mismo
	sigemptyset(&accion.sa_mask);
	sigaddset(&accion.sa_mask, SIGCHLD);

	if (sigaction(SIGCHLD, &accion, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}
}

// runs a shell command
static void
run_shell()
{
	char *cmd;

	while ((cmd = read_line(prompt)) != NULL)
		if (run_cmd(cmd) == EXIT_SHELL)
			return;
}

// initializes the shell
// with the "HOME" directory
static void
init_shell()
{
	char buf[BUFLEN] = { 0 };
	char *home = getenv("HOME");

	if (chdir(home) < 0) {
		snprintf(buf, sizeof buf, "cannot cd to %s ", home);
		perror(buf);
	} else {
		snprintf(prompt, sizeof prompt, "(%s)", home);
	}
}

int
main(void)
{
	init_shell();
	init_signals();

	run_shell();

	return 0;
}