/**
* @file syserror.c
* @brief Chamadas de sistema com verificação de erro.
* @see syserror.h
**/
#include "syserror.h"

pid_t Fork(void)
{
	int pid = -1;
	if ((pid = fork()) < 0)
	{
		erroUnix("fork error");
		exit(0);
	}
	return pid;
}

int Pipe(int p[])
{
	int ret = -1;
	if ((ret = pipe(p)) < 0)
	{
		erroUnix("pipe error");
		exit(0);
	}
	return ret;
}

void Kill(pid_t pid, int sig)
{
	if (kill(pid, sig) < 0)
	{
		erroUnix("kill error");
		exit(0);
	}
}

pid_t Setpgid(pid_t pid, pid_t pgid)
{
	pid_t ret = 1;
	if ((ret = setpgid(pid, pgid)) < 0)
	{
		erroUnix("setpgid error");
	}
	return ret;
}

int Sigaddset(sigset_t *set, int signum)
{
	int ret;
	if((ret = sigaddset(set,signum)) < 0)
	{
		erroUnix("sigaddset error");
	}
	return ret;
}

int Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	int ret = 1;
	if((ret = sigprocmask(how,set,oldset)) < 0)
	{
		erroUnix("sigprocmask error");
	}
	return ret;
}

int Sigemptyset(sigset_t *set)
{
	int ret;
	if((ret = sigemptyset(set)) < 0)
	{
		erroUnix("sigemptyset error");
	}	
	return ret;
}

pid_t Waitpid(pid_t pid, int *status, int options)
{
	pid_t ret;
	if ((ret = waitpid(pid,status,options) ) < 0)
	{
		erroUnix("waitpid error");
		exit(0);
	}
	return ret;
}

void erroUnix(char *msg)
{
	fprintf(stdout, "%s: %s\n", msg, strerror(errno));
	exit(1);
}
