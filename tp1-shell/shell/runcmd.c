#include "runcmd.h"

int status = 0;

// runs the command in 'cmd'
int
run_cmd(char *cmd)
{
	pid_t p;
	struct cmd *parsed;
	sigset_t mascara_sigchld, mascara_anterior;

	// if the "enter" key is pressed
	// just print the prompt again
	if (cmd[0] == END_STRING)
		return 0;

	// "history" built-in call
	if (history(cmd))
		return 0;

	// "cd" built-in call
	if (cd(cmd))
		return 0;

	// "exit" built-in call
	if (exit_shell(cmd))
		return EXIT_SHELL;

	// "pwd" built-in call
	if (pwd(cmd))
		return 0;

	// parses the command line
	parsed = parse_line(cmd);

	// IMPLEMENTACION
	// Bloqueamos SIGCHLD antes de forkear, y lo dejamos bloqueado
	// hasta terminar de decidir el grupo de procesos del hijo, ver
	// los dos setpgid() de mas abajo.
	//
	// Como lo pense: si el hijo termina MUY rapido, pensemos en algo como
	// "/bin/true", el kernel podria entregarnos el SIGCHLD y
	// disparar sigchld_handler(), definido en sh.c antes de que
	// lleguemos a sacarlo del grupo de procesos de la shell. Si eso
	// pasara con un comando de primer plano, el handler, que solo
	// deberia reapear procesos de segundo plano, se lo podria "robar"
	// con su propio waitpid(), y entonces el waitpid(p, &status, 0)
	// de mas abajo se quedaria esperando por un hijo que ya no
	// existe. Bloqueando la señal, si el hijo termina en ese
	// interin, el SIGCHLD queda simplemente pendiente y recien se
	// entrega cuando la desbloqueamos, con el grupo ya bien
	// configurado.
	sigemptyset(&mascara_sigchld);
	sigaddset(&mascara_sigchld, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mascara_sigchld, &mascara_anterior);

	// forks and run the command
	if ((p = fork()) == 0) {
		// A todo proceso de primer plano, cualquiera que no
		// sea BACK, lo mandamos a su propio grupo de procesos,
		// usando su propio pid como id de grupo, eso es lo que
		// hace setpgid(0, 0): "mi grupo pasa a ser mi propio
		// pid". A los de segundo plano no los tocamos: se quedan
		// en el mismo grupo que la shell, que es el que
		// heredan por default al hacer fork().
		//
		// Para que sirve esto?: el handler de SIGCHLD (sh.c)
		// reapea procesos llamando a waitpid(0, ...etc), y ese
		// "0" significa "espera por cualquier hijo cuyo grupo
		// sea igual al MiO", el de quien llama, o sea la
		// propia shell. Si dejaramos a los procesos de primer
		// plano en el grupo de la shell, entrarian en esa
		// categoria y el handler se los podria reapear antes
		// de tiempo. Separandolos en su propio grupo, solo
		// quedan al alcance del handler los de segundo plano, que
		// son los unicos que de verdad queremos que reapee el.
		if (parsed->type != BACK)
			setpgid(0, 0);

		// El hijo heredo, via fork(), el bloqueo de SIGCHLD
		// que acabamos de hacer mas arriba. Se lo revertimos
		// antes de ejecutar el programa pedido, dentro de
		// exec_cmd, para que ese programa reciba señales de
		// forma normal, y no cargue con una restriccion que
		// solo tenia sentido para la shell.
		sigprocmask(SIG_SETMASK, &mascara_anterior, NULL);

		exec_cmd(parsed);
	}

	// stores the pid of the process
	parsed->pid = p;

	// Parecido al setpgid() de mas arriba, pero del lado de la shell
	// (el padre). Hace falta hacerlo en los dos lugares porque no
	// hay garantia de si el padre o el hijo se ejecutan primero
	// despues del fork(): la primera de las dos llamadas "gana", y
	// la segunda es simplemente lo mismo, mover un proceso a un
	// grupo que ya tiene es un no-op.
	if (parsed->type != BACK)
		setpgid(p, p);

	// Ya quedo definido el grupo de procesos del hijo, asi que es
	// seguro volver a dejar pasar SIGCHLD.
	sigprocmask(SIG_SETMASK, &mascara_anterior, NULL);

	// background process special treatment
	if (parsed->type == BACK) {
		// avisamos con que pid quedo corriendo en 2do plano...
		print_back_info(parsed);

		// ...y no lo esperamos aca:
		// terminamos con un comando con "&", devolver el control a
		// la shell de inmediato en vez de bloquearse. Quien se
		// entera de que el proceso termino, mas adelante, es
		// sigchld_handler() (sh.c). No necesitamos conservar
		// nada mas del arbol de comandos parseado, ya usamos su
		// pid en print_back_info(), asi que lo liberamos ahora
		// mismo en vez de esperar a la proxima linea.
		free_command(parsed);

		return 0;
	}

	// waits for the process to finish
	waitpid(p, &status, 0);

	print_status_info(parsed);

	free_command(parsed);

	return 0;
}