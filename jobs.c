/**
* @file jobs.c
* @brief Funções para manipulação da estrutura de jobs.
* @see jobs.h
**/
#include "jobs.h"
#include "syserror.h"
/**
* @brief Fica num while até que o processo que roda em foreground mude de estado.
* @param pid do processo.
* @see getJobPid()
*/
void esperaFg(pid_t pid)
{
	struct job_t* job;
	while ((job=getJobPid(jobs,pid))!=NULL&&(job->estado==FG)){
		sleep(0);
	}
}
/**
* @brief Zera todos os campos da estrutura de um job.
* @param job um ponteiro para o mesmo.
*/
void zeraJob(struct job_t *job) 
{
	job->pid = 0;
	job->jid = 0;
	job->estado = UNDEF;
	job->cmdline[0] = '\0';
}
/**
* @brief Inicia todos os jobs zerados.
* @param Coleção jobs.
* @see zeraJob()
*/
void iniciaJobs(struct job_t *jobs) 
{
	int i;
	for (i = 0; i < MAXJOBS; i++)
		zeraJob(&jobs[i]);
}
/**
* @brief Retorna o maior Jid.
* @param Coleção jobs.
*/
int maxJid(struct job_t *jobs)
{
	int i, max=0;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].jid > max)
			max = jobs[i].jid;
	return max;
}
/**
* @brief Adiciona um novo job.
* @param Coleção jobs.
* @param pid do novo processo.
* @param estado em que vai ser inserido.
* @param cmdline o nome do job (comando).
* @return int sucesso ou fracasso.
*/
int adicionaJob(struct job_t *jobs, pid_t pid, int estado, char *cmdline)
{
	int i;
	if (pid < 1)
		return 0;
	for (i = 0; i < MAXJOBS; i++) 
	{
		if (jobs[i].pid == 0) 
		{
			jobs[i].pid = pid;
			jobs[i].estado = estado;
			jobs[i].jid = proxJid++;
			if (proxJid > MAXJOBS)
				proxJid = 1;
			strcpy(jobs[i].cmdline, cmdline);
			return 1;
		}
	}
	printf("Ja existem jobs demais!\n");
	return 0;
}
/**
* @brief Remove um job da coleção de jobs.
* @param Coleção de jobs.
* @param pid do processo a ser excluido.
* @return int sucesso ou fracasso.
*/
int excluiJob(struct job_t *jobs, pid_t pid)
{
	int i;
	if (pid < 1)
		return 0;
	for (i = 0; i < MAXJOBS; i++) 
	{
		if (jobs[i].pid == pid) 
		{
			zeraJob(&jobs[i]);
			proxJid = maxJid(jobs)+1;
			return 1;
		}
	}
	return 0;
}
/**
* @brief Fica num while até que o processo que roda em foreground mude de estado.
* @param pid do processo.
* @see getJobPid()
*/
struct job_t *getJobPid(struct job_t *jobs, pid_t pid) 
{
	int i;
	if (pid < 1)
		return NULL;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].pid == pid)
	    		return &jobs[i];
	return NULL;
}
/**
* @brief Pega o JID atraves de um pid.
* @param pid do processo.
*/
int pidToJid(pid_t pid)
{
	int i;
	if (pid < 1)
		return 0;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].pid == pid)
			return jobs[i].jid;
	return 0;
}
/**
* @brief Imprime todos os jobs da coleção.
* @param Coleção de jobs.
*/
void listaJobs(struct job_t *jobs)
{
	int i;
	
	printf("[JOBID] (PID)\n");
	for (i = 0; i < MAXJOBS; i++) 
	{
		if (jobs[i].pid != 0) 
		{
			printf("[%d]     (%d) ", jobs[i].jid, jobs[i].pid);
			switch (jobs[i].estado) 
			{
				case BG:
					printf("Background ");
				break;
				case FG:
					printf("Foreground ");
				break;
				case ST:
					printf("Parado     ");
				break;
				default:
					printf("listaJobs: Internal error: job[%d].estado=%d ",i,jobs[i].estado);
			}
			printf("%s", jobs[i].cmdline);
		}
	}
}
/**
* @brief Adiciona um sinal a um manipulador.
* @param handler manipulador ja existente.
* @param signum sinal que sera adicionado.
*/
handler_t *Signal(int signum, handler_t *handler)
{
	struct sigaction action, old_action;

	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESTART;

	if (sigaction(signum, &action, &old_action) < 0)
		erroUnix("Signal error");
	return (old_action.sa_handler);
}
/**
* @brief Quando o sinal ^C é recebido dá kill.
* @param sig sinal.
* @see Kill()
*/
void manipulaCtrlC(int sig)
{
	int i;
	for (i = 0; i < MAXJOBS; i++)
	{
		if (jobs[i].estado == FG)
		{
			Kill(-jobs[i].pid,SIGINT);
		}
	}
}
/**
* @brief Quando o sinal ^Z é recebido dá kill.
* @param sig sinal.
* @see Kill()
*/
void manipulaCtrlZ(int sig)
{
	int i;
	for (i = 0; i < MAXJOBS; i++)
	{
		if (jobs[i].estado == FG)
		{
			Kill(-jobs[i].pid,SIGTSTP);
		}
	}
}
/**
* @brief Verifica os sinais do processo em espera.
* @param sig sinal.
* @see excluiJob()
*/
void manipulaChld(int sig)
{
	int pid=0;
	int status=-1;
	do{     
		pid = waitpid(-1,&status,WNOHANG|WUNTRACED);
		if(pid > 0)
		{
			if(WIFEXITED(status))
			{	/**< Filho foi finalizado normalmente. */
				excluiJob(jobs,pid);
			}
			else if(WIFSIGNALED(status))
			{  	/**< Filho foi finalizado incorretamente, ^C */
				if(WTERMSIG(status) == 2){
					printf("\n Job [%d] (%d) encerrado!\n", pidToJid(pid), pid);
					excluiJob(jobs,pid);
				}
			}
			else if(WIFSTOPPED(status))
			{    	/**< Filho foi parado, ^Z */
				getJobPid(jobs, pid)->estado=ST;
				printf("\n Job [%d] (%d) parado!\n", pidToJid(pid), pid);
			}
		}
	}while(pid > 0);
}
