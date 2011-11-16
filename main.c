/**
* @file jobs.h
* @brief Função principal e comandos built in.
* @see jobs.h
* @see processo.h
* @see syserror.h
**/
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "syserror.h"
#include "jobs.h"
#include "processo.h"
/**
* Variáveis globais
**/
int proxJid = 1;
sigset_t maskchld; 		/**< Mascara de bloqueio de sinais.*/
char *ptr, *ag;			/**< Caminho do shell e diretorio corrente.*/
pid_t shell_pgid;		
int shell_terminal;		
struct job_t jobs[MAXJOBS];	/**< Coleção de jobs.*/
/**
* Protótipos funções builtIn e auxiliares
**/
void executa(char *);
int builtIn(char **, int);
void BgFgKill(char **, int);
void cd(char **, int);
int checkDirPipe(char *);
/**
* @brief Função principal, while infinito e le/executa comando.
* @param pid do processo.
* @see executa()
* @see Signal()
* @see Sigemptyset()
* @see Sigaddset()
*/
int main(int argc, char **argv)
{
	char *bu;
	char cmdline[MAXLINE];

	/**< Criando mascara de bloqueio.*/
	Sigemptyset(&maskchld);
	Sigaddset(&maskchld, SIGCHLD);
	Sigaddset(&maskchld, SIGTSTP);

	shell_terminal = STDIN_FILENO;
	
	/**< Pegando o diretorio de execução do shell.*/
	long size = pathconf(".", _PC_PATH_MAX);
	if ((bu = (char *)malloc((size_t)size)) != NULL)
		ptr = getcwd(bu, (size_t)size);

	dup2(1, 2); /**< Redireciona stderr para stdout, para o uso do pipe.*/

	/**< Ignorando sinais de controle de JOB.*/
	Signal(SIGINT, SIG_IGN);
	Signal(SIGTSTP, SIG_IGN);
	Signal(SIGCHLD, SIG_IGN);	
	/**< Ligando os sinais aos manipuladores*/
	Signal(SIGINT,  manipulaCtrlC);
	Signal(SIGTSTP, manipulaCtrlZ);
	Signal(SIGCHLD, manipulaChld);

	while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
		Kill(-shell_pgid, SIGTTIN);
	/**< Colocando o shell em seu próprio grupo.*/
	shell_pgid = getpid();
	Setpgid(shell_pgid, shell_pgid);
	tcsetpgrp(shell_terminal, shell_pgid);
	/**< Iniciando coleção de jobs*/
	iniciaJobs(jobs);

	while (1) {
		/**< Pegando diretorio corrente e printa nome do shell.*/
		char *buf;
		long size = pathconf(".", _PC_PATH_MAX);
		if ((buf = (char *)malloc((size_t)size)) != NULL)
			ag = getcwd(buf, (size_t)size);
		if(!strcmp(ag,ptr))
			printf("\033[94m[PRIMOSH]~$ \033[00m");
		else
			printf("\033[94m[PRIMOSH]~%s$ \033[00m",ag);
		/**< Le (fgets) a linha digitada e verifica erros.*/
		if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
		{
			printf("FGETS ERROR!\n");
			exit(0);
		}
		if (feof(stdin)) { /**< ctrl-d.*/
			fflush(stdout);
			exit(0);
		}
		/**< Se foi digitada alguma coisa.*/
		if(strlen(cmdline) > 1)
		{	
			/**< Executa a linha e religa os sinais aos manipuladores.*/
			executa(cmdline);
			Signal(SIGINT,  manipulaCtrlC);
			Signal(SIGTSTP, manipulaCtrlZ);
			Signal(SIGCHLD, manipulaChld);
		}
	}
	exit(0); 
}
/**
* @brief Direciona o processo a função correta.
* @param cmdline - linha de comando digitada
* @see parser()
* @see builtIn()
* @see checkDirPipe()
* @see procBg()
* @see procFg()
* @see setaDir()
* @see procDirPipe()
* @see redir()
*/
void executa(char *cmdline)
{
	char *argv[MAXARGS];
	int argc = 0;
	/**< Verifica se rodara em background.*/
	int bg = parser(cmdline,argv,&argc);
	/**< Verifica se é comando built in, se for a propria função processara.*/
	if(builtIn(argv,argc))
		return;
	/**< Bloqueia os sinais com a mascara criada.*/
	//Sigprocmask(SIG_BLOCK,&maskchld,NULL);
	/**< Verifica se comando nao possui pipe/redirecionamento.*/
	if(!checkDirPipe(cmdline))
	{	
		if(bg) /**< Executa em background.*/
			procBg(argv);
		else   /**< Executa em foreground.*/
			procFg(argv);
	}
	else /**< Possui pipe/redireciomanto.*/
	{
		int fdstd[2];
		setaDir(cmdline);
		procDirPipe(cmdline, 0, NULL, fdstd);
		limpaFlags();
	}
	return;
}
/**
* @brief Verifica se comando possui pipe/redirecionamento.
* @param cmdline - linha de comando digitada
* @return int possui ou nao possui.
*/
int checkDirPipe(char *cmdline)
{
	char **argv;
	int k, size, i, ct;
	char c, arg[MAXLINE];

	argv = (char**) calloc( MAXARGS, MAXLINE );
	size = strlen(cmdline);
	cmdline[size] = '\0';
	ct = 0;
	k = 0;
	do{
		argv[ct]=(char *)malloc(sizeof(arg));
		i = 0;
		while((c = cmdline[k]) != ' ')
		{
			if(c == '\0')
				break;
			*(argv[ct]+i) = c;
		    	i++;
		    	k++;
		}
		*(argv[ct]+i) = '\0';
		ct++;
		k++;
	}while(c != '\0');

	for( i = 0; i < ct; i++ )
	{
		if(!strcmp(argv[i],">")||!strcmp(argv[i],"<<")||!strcmp(argv[i],"<")||!strcmp(argv[i],">>")||
!strcmp(argv[i],"|")||!strcmp(argv[i],"1>")||!strcmp(argv[i],"2>")||!strcmp(argv[i],"1>>")||!strcmp(argv[i],"2>>"))
			return 1;
	}
	return 0;
}
/**
* @brief Verifica se primeiro comando digitado e built in.
* @param cmdline - linha de comando digitada
* @param numberofargs - numero de argumentos do comando
* @return int - é built in ou não
* @see BgFgKill()
* @see cd()
* @see listaJobs()
*/
int builtIn(char **argv, int numberofargs)
{
	int i;

	if(!strcmp(argv[0],"exit"))
		exit(EXIT_SUCCESS);
	else if(!strcmp(argv[0],"fg") || !strcmp(argv[0],"bg") || !strcmp(argv[0],"kill"))
	{
		BgFgKill(argv,numberofargs);
		return 1;
	}
	else if(!strcmp(argv[0],"cd"))
	{
		cd(argv,numberofargs);
		return 1;
	}
	else if(!strcmp(argv[0],"pwd"))
	{
		printf("%s\n", ag);		/**< Imprime o diretorio corrente.*/
		return 1;
	}
	else if(!strcmp(argv[0],"echo"))
	{
		for(i = 1 ; i < numberofargs ; i++) 
			printf("%s ", argv[i]); /**< Imprime o que foi digitado depois de echo.*/
		printf("\n");
	return 1;
	}
	else if(!strcmp(argv[0],"jobs"))
	{
		listaJobs(jobs);
		return 1;
	}
	return 0;
}
/**
* @brief Usa chdir pra mudar o diretorio corrente.
* @param argv comandos digitados.
* @param tam numero de argumentos
*/
void cd(char **argv, int tam)
{
	char *pt;
	int i;

	if(tam > 2 )
	{  /**< Caminho com espaço no nome de diretorio.*/
		if((pt = (char *)malloc(200)) != NULL)
		{
			for(i = 1 ;i < tam ;i++)
			{
				if(i != 1)
				strcat(pt," ");
				strcat(pt,argv[i]);
			}
		}
		if (chdir(pt) < 0)
			printf("cd: %s: Diretório não encontrado.\n", pt);
	}
	else if(tam == 2)
	{ /**< Caminho sem espaço no nome do diretorio.*/
		if (chdir(argv[1]) < 0)
			printf("cd: %s: Diretório não encontrado.\n", argv[1]);
	}
	else if(tam == 1)
	{ /**< CD sem caminho, volta pra diretorio do shell(onde tá rodando).*/
		if (chdir(ptr) < 0)
			printf("cd: %s: Diretório não encontrado.\n", ptr);
	}
}
/**
* @brief Coloca processo em foreground/background ou o mata.
* @param argv comandos digitados.
* @param numberofargs numero de argumentos
* @see Kill()
* @see esperaFG()
*/
void BgFgKill(char **argv, int numberofargs)
{
	int pid, jid;
	int i = 0;

	if(numberofargs > 1)
	{
		if( argv[1][0] == '%' )
		{       /**< Foi digitado %JID.*/
			jid = atoi(argv[1]+1);
			while( (i < MAXJOBS) && (jobs[i].jid != jid) )
				i++;	/**< Procura o indice i do job.*/
		}
		else	/**< Foi digitado o PID.*/
		{
			pid = atoi(argv[1]);
			if(pid <= 0)
			{
				printf("%s: argumento deve ser o PID ou %%jobid\n",argv[0]);
				return;
			}
			else
			{
				while( (i < MAXJOBS) && (jobs[i].pid != pid) )
					i++;	/**< Procura o indice i do job.*/
			}
		}

		if( (i >= 0) && (i < MAXJOBS) )
		{
			int actualstate = jobs[i].estado;
			/**< Processo parado e foi digitado bg.*/
			if ( (actualstate == ST) && !strcmp(argv[0],"bg") )
			{	
				Kill(-jobs[i].pid, SIGCONT); /**< Manda sinal para continuar.*/
				jobs[i].estado = BG;	     /**< Muda o estado para BG.*/	
				printf("[%d] (%d) %s", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
			}
			/**< Se foi digitado fg.*/
			else if(!strcmp(argv[0],"fg"))
			{
				Kill(-jobs[i].pid, SIGCONT); /**< Manda sinal para continuar.*/
				jobs[i].estado = FG;	     /**< Muda o estada para FG.*/
				esperaFg(jobs[i].pid);	     /**< Espera ate que seja finalizado ou parado.*/
			}
			/**< Se estado em BG e foi digitado kill.*/
			else if(actualstate == BG && !strcmp(argv[0],"kill"))
			{
				if(!kill(-jobs[i].pid, SIGTERM)) /**< Se sinal de termino enviado com sucesso.*/
				{
					printf(" Job [%d] (%d) encerrado!\n", jobs[i].jid, jobs[i].pid);
					excluiJob(jobs,jobs[i].pid); /**< Exclui job da coleção.*/
					proxJid = maxJid(jobs)+1;    /**< Seta qual sera o proximo JID.*/
				}
				else	/**< Sinal não enviado com sucesso.*/
				{
					fprintf(stderr," Job [%d] (%d) nao pode ser encerrado\n", jobs[i].jid, jobs[i].pid);
				}
			}
		}
		else	/**< Não encontrou o job.*/
		{
			if( argv[1][0] == '%' )
			{
				printf("%s: Job nao encontrado\n", argv[1]);
			}
			else
			{
				printf("(%s): Processo nao encontrado\n", argv[1]);
			}
		}
	}
	else /**< Não foi digitado o JID.*/
		printf("%s: argumento deve ser o PID ou %%jobid\n",argv[0]);
}
