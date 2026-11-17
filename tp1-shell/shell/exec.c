#include "exec.h"

// sets "key" with the key part of "arg"
// and null-terminates it
static void
get_environ_key(char *arg, char *key)
{
	int i;

	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets "value" with the value part of "arg"
// and null-terminates it
static void
get_environ_value(char *arg, char *value, int idx)
{
	size_t i, j;

	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
static void
set_environ_vars(char **eargv, int eargc)
{
	for (int i = 0; i < eargc; i++) {
		char key[BUFLEN];
		char value[BUFLEN];

		int idx = block_contains(eargv[i], '=');

		get_environ_key(eargv[i], key);
		get_environ_value(eargv[i], value, idx);

		setenv(key, value, 1);
	}
}

// opens the file in which the stdin/stdout/stderr
// flow will be redirected, and returns
// the file descriptor
static int
open_redir_fd(char *file, int flags)
{
	int fd;

	if (flags & O_CREAT) {
		fd = open(file, flags, 0644);
	} else {
		fd = open(file, flags);
	}

	if (fd < 0) {
		fprintf(stderr, "Error: fallo el open de %s", file);
	}

	return fd;
}

// executes a command - does not return
void
exec_cmd(struct cmd *cmd)
{
	struct execcmd *e;
	struct backcmd *b;
	struct execcmd *r;
	struct pipecmd *p;

	switch (cmd->type) {
	case EXEC:
		e = (struct execcmd *) cmd;

		set_environ_vars(e->eargv, e->eargc);

		execvp(e->argv[0], e->argv);

		fprintf(stderr, "exec failed : %s\n", strerror(errno));
		_exit(-1);
		break;

	case BACK: {
		/*
		 * El proceso que ejecuta en segundo plano ya fue creado
		 * por run_cmd(). El backcmd solamente envuelve al comando
		 * real, por lo que aca lo desenvolvemos y ejecutamos.
		 */
		b = (struct backcmd *) cmd;

		exec_cmd(b->c);

		break;
	}

	case REDIR: {
		r = (struct execcmd *) cmd;

		if (strlen(r->in_file) > 0) {
			int fd_entrada = open_redir_fd(r->in_file, O_RDONLY);

			if (fd_entrada < 0) {
				_exit(-1);
			}

			dup2(fd_entrada, 0);
			close(fd_entrada);
		}

		if (strlen(r->out_file) > 0) {
			int fd_salida =
			        open_redir_fd(r->out_file,
			                      O_WRONLY | O_CREAT | O_TRUNC);

			if (fd_salida < 0) {
				_exit(-1);
			}

			dup2(fd_salida, 1);
			close(fd_salida);
		}

		if (strlen(r->err_file) > 0) {
			if (strcmp(r->err_file, "&1") == 0) {
				dup2(1, 2);
			} else {
				int fd_error = open_redir_fd(r->err_file,
				                             O_WRONLY | O_CREAT |
				                                     O_TRUNC);

				if (fd_error < 0) {
					_exit(-1);
				}

				dup2(fd_error, 2);
				close(fd_error);
			}
		}

		set_environ_vars(r->eargv, r->eargc);
		execvp(r->argv[0], r->argv);

		fprintf(stderr, "%s: %s\n", r->argv[0], strerror(errno));

		_exit(-1);
		break;
	}

	case PIPE: {
		p = (struct pipecmd *) cmd;

		/*
		 * El proceso coordinador del pipe espera explicitamente
		 * a sus dos hijos con waitpid(). Por eso no debe tener
		 * el handler de SIGCHLD de la shell activo.
		 */
		signal(SIGCHLD, SIG_DFL);

		int pipe_fds[2];

		if (pipe(pipe_fds) < 0) {
			fprintf(stderr, "pipe: %s\n", strerror(errno));
			_exit(-1);
		}

		int pid_izquierdo = fork();

		if (pid_izquierdo < 0) {
			fprintf(stderr, "Error: fallo el fork\n");

			close(pipe_fds[READ]);
			close(pipe_fds[WRITE]);

			free_command(cmd);
			_exit(-1);
		}

		if (pid_izquierdo == 0) {
			close(pipe_fds[READ]);

			dup2(pipe_fds[WRITE], 1);

			close(pipe_fds[WRITE]);

			exec_cmd(p->leftcmd);
		}

		int pid_derecho = fork();

		if (pid_derecho < 0) {
			fprintf(stderr, "Error: fallo el fork\n");

			close(pipe_fds[READ]);
			close(pipe_fds[WRITE]);

			waitpid(pid_izquierdo, NULL, 0);

			free_command(cmd);
			_exit(-1);
		}

		if (pid_derecho == 0) {
			close(pipe_fds[WRITE]);

			dup2(pipe_fds[READ], 0);

			close(pipe_fds[READ]);

			exec_cmd(p->rightcmd);
		}

		close(pipe_fds[READ]);
		close(pipe_fds[WRITE]);

		waitpid(pid_izquierdo, NULL, 0);
		waitpid(pid_derecho, NULL, 0);

		free_command(cmd);

		_exit(0);
		break;
	}
	}
}
