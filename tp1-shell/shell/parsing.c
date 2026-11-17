#include "parsing.h"

// parses an argument of the command stream input
static char *
get_token(char *buf, int idx)
{
	char *tok;
	int i;

	tok = (char *) calloc(ARGSIZE, sizeof(char));
	i = 0;

	while (buf[idx] != SPACE && buf[idx] != END_STRING) {
		tok[i] = buf[idx];
		i++;
		idx++;
	}

	return tok;
}

static bool
parse_redir_flow(struct execcmd *c, char *arg)
{
	int inIdx, outIdx;

	if ((outIdx = block_contains(arg, '>')) >= 0) {
		switch (outIdx) {
		case 0: {
			strcpy(c->out_file, arg + 1);
			break;
		}

		case 1: {
			strcpy(c->err_file, &arg[outIdx + 1]);
			break;
		}
		}

		free(arg);
		c->type = REDIR;

		return true;
	}

	if ((inIdx = block_contains(arg, '<')) >= 0) {
		strcpy(c->in_file, arg + 1);

		c->type = REDIR;
		free(arg);

		return true;
	}

	return false;
}

static bool
parse_environ_var(struct execcmd *c, char *arg)
{
	if (block_contains(arg, '=') > 0) {
		if (block_contains(arg, '-') < 0) {
			c->eargv[c->eargc++] = arg;
			return true;
		}
	}

	return false;
}

// this function will be called for every token, and it should
// expand environment variables. In other words, if the token
// happens to start with '$', the correct substitution with the
// environment value should be performed. Otherwise the same
// token is returned. If the variable does not exist, an empty string should be
// returned within the token
//
// Hints:
// - check if the first byte of the argument contains the '$'
// - expand it and copy the value in 'arg'
// - remember to check the size of variable's value
//		It could be greater than the current size of 'arg'
//		If that's the case, you should realloc 'arg' to the new size.

extern int status;

static char *
expand_environ_var(char *arg)
{
	if (arg == NULL || arg[0] != '$')
		return arg;

	char *var_name = arg + 1;
	char *var_value = NULL;
	char buffer[16];

	if (strcmp(var_name, "?") == 0) {
		snprintf(buffer, sizeof(buffer), "%d", status);
		var_value = buffer;
	}

	else {
		var_value = getenv(var_name);
		if (var_value == NULL)
			var_value = "";
	}

	size_t new_val_size = strlen(var_value);
	arg = realloc(arg, new_val_size + 1);
	strcpy(arg, var_value);

	return arg;
}

// parses one single command having into account:
// - the arguments passed to the program
// - stdin/stdout/stderr flow changes
// - environment variables (expand and set)
static struct cmd *
parse_exec(char *buf_cmd)
{
	struct execcmd *c;
	char *tok;
	int idx = 0, argc = 0;

	c = (struct execcmd *) exec_cmd_create(buf_cmd);

	while (buf_cmd[idx] != END_STRING) {
		tok = get_token(buf_cmd, idx);
		idx = idx + strlen(tok);

		if (buf_cmd[idx] != END_STRING)
			idx++;

		if (parse_redir_flow(c, tok))
			continue;

		if (parse_environ_var(c, tok))
			continue;

		tok = expand_environ_var(tok);

		if (tok[0] |= END_STRING)
			c->argv[argc++] = tok;
	}

	c->argv[argc] = (char *) NULL;
	c->argc = argc;

	return (struct cmd *) c;
}

static struct cmd *
parse_back(char *buf_cmd)
{
	int i = 0;
	struct cmd *e;

	while (buf_cmd[i] != '&')
		i++;

	buf_cmd[i] = END_STRING;

	e = parse_exec(buf_cmd);

	return back_cmd_create(e);
}


static struct cmd *
parse_cmd(char *buf_cmd)
{
	if (strlen(buf_cmd) == 0)
		return NULL;

	int idx;

	if ((idx = block_contains(buf_cmd, '&')) >= 0 && buf_cmd[idx - 1] != '>')
		return parse_back(buf_cmd);

	return parse_exec(buf_cmd);
}


// Tuberias multiples
// parses the command line
// looking for the pipe character '|'
struct cmd *
parse_line(char *buf)
{
	struct cmd *r, *l;

	char *right = split_line(buf, '|');

	l = parse_cmd(buf);

	// si no queda nada despues del '|' (o directamente no habia
	// ningún '|' en la linea), no hay mas pipes que resolver:
	// devolvemos el comando de la izquierda tal cual, sin
	// envolverlo en un pipecmd
	if (strlen(right) == 0)
		return l;

	// puede haber mas de un '|' todavia en 'right' (ej: si la
	// linea original era "a | b | c", aca 'right' es "b | c").
	// Lo resolvemos llamando de nuevo a parse_line() —en vez de
	// parse_cmd(), que solo entiende UN comando— para que siga
	// partiendo recursivamente cada vez que encuentre un '|',
	// hasta que no quede ninguno. Asi se arma un arbol asociando
	// a la derecha: a | (b | (c | ...))
	r = parse_line(right);

	return pipe_cmd_create(l, r);
}