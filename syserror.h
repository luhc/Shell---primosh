/**
* @file syserror.h
* @brief Prototipos das funções e bibliotecas.
* @see syserro.c
**/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
/**
* Protótipos das funções implementadas.
**/
pid_t Fork(void);
int Pipe(int *);
void Kill(pid_t, int );
pid_t Setpgid(pid_t, pid_t );
int Sigaddset(sigset_t *, int);
int Sigprocmask(int, const sigset_t *, sigset_t *);
int Sigemptyset(sigset_t *);
pid_t Waitpid(pid_t, int *, int);
void erroUnix(char *);
