/**
* @file jobs.h
* @brief Prototipos das funções, bibliotecas, defines, struct job.
* @see jobs.c
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
* Algumas definições.
**/
#define MAXLINE    1000
#define MAXARGS      80
#define MAXJOBS      32
#define MAXJID    1<<32
/**
* Possíveis estados de um processo.
**/
#define UNDEF 0 /**< Indefinido.*/
#define FG 1    /**< Foreground.*/
#define BG 2    /**< Background.*/
#define ST 3    /**< Parado.*/

struct job_t {               /**< Struct do job.*/
    pid_t pid;               /**< ID processo.*/
    int jid;                 /**< Job ID.*/
    int estado;              /**< Estado acima.*/
    char cmdline[MAXLINE];   /**< Nome do comando.*/
};
typedef void handler_t(int); /**< Outro definição, para melhor programação.*/
/**
* Protótipos das funções implementadas.
**/
void listaJobs(struct job_t *);
int pidToJid(pid_t);
struct job_t *getJobPid(struct job_t *, pid_t);
int excluiJob(struct job_t *, pid_t);
int adicionaJob(struct job_t *, pid_t, int, char *);
int maxJid(struct job_t *);
void iniciaJobs(struct job_t *);
void zeraJob(struct job_t *);
void esperaFg(pid_t);
handler_t *Signal(int, handler_t *);
void manipulaCtrlZ(int);
void manipulaCtrlC(int);
void manipulaChld(int);
void Wait();
/**
* Variáveis externas.
**/
extern int proxJid;
extern struct job_t jobs[MAXJOBS];
extern pid_t shell_pgid;
extern int shell_terminal;
