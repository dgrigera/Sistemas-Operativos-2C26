#include "builtin.h"
extern int status;

// returns true if the 'exit' call
// should be performed
//
// (It must not be called from here)
int
exit_shell(char *cmd)
{
	if (strcmp(cmd, "exit") == 0) {
		return true;
	}

	return false;
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to $HOME)
//  it has to be executed and then return true
//
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int
cd(char *cmd)
{
	if (strncmp(cmd, "cd ", 3) == 0) {
		int indice = (block_contains(cmd, ' ') + 1);
		char directorio[50];

		strcpy(directorio, cmd + indice);

		if (chdir(directorio) == -1) {
			fprintf(stderr,
			        "cannot cd to %s : %s\n",
			        directorio,
			        strerror(errno));
			status = 1;
		} else {
			status = 0;
			char cwd[PRMTLEN];
			if (getcwd(cwd, sizeof(cwd)) != NULL) {
				snprintf(prompt, sizeof(prompt), "(%s)", cwd);
			}
		}
		return true;

	} else if (strcmp(cmd, "cd") == 0) {
		if (chdir(getenv("HOME")) == -1) {
			fprintf(stderr,
			        "cannot cd to %s : %s\n",
			        getenv("HOME"),
			        strerror(errno));
			status = 1;
		} else {
			status = 0;
			char cwd[PRMTLEN];
			if (getcwd(cwd, sizeof(cwd)) != NULL) {
				snprintf(prompt, sizeof(prompt), "(%s)", cwd);
			}
		}
		return true;

	} else {
		return false;
	}
}

// returns true if 'pwd' was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
pwd(char *cmd)
{
	char buffer[1024];

	if (strcmp(cmd, "pwd") == 0) {
		if (getcwd(buffer, sizeof(buffer)) != NULL) {
			printf("%s\n", buffer);
			return true;
		} else {
			fprintf(stderr,
			        "cannot get current working directory : %s\n",
			        strerror(errno));
		}
	}

	return false;
}

// returns true if `history` was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
history(char *cmd)
{
	// Your code here

	return 0;
}
