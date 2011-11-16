/**
* @file processo.h
* @brief Prototipos das funções e bibliotecas.
* @see processo.c
**/
#include <signal.h>
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
int parser(const char *, char **, int *);
void procBg(char **);
void procFg(char **);
void procDirPipe(char *, int , int ifd[2], int fdstd[2]);
void setaDir(char *);
void limpaFlags();
