/**
* @file processo.c
* @brief Funções de processo e parser.
* @see jobs.h
* @see processo.h
* @see syserror.h
**/
#include "jobs.h"
#include "syserror.h"
/**
* Variáveis globais externas.
**/
extern int nPaths;
extern char** PATHS;
extern sigset_t maskchld;
extern pid_t shell_pgid;
extern int shell_terminal;
/**
* Variáveis para redirecionamento.
**/ 
int redir_out = 0;
int redir_in = 0; 
int redir_app = 0;
int redir_err = 0;
int redir_erra = 0;
int fpin, fpout, fperr; /**< Ponteiros de arquivos.*/
fpos_t pos_in, pos_out; /**< Posicao do buffer de stdin e stdout.*/
/**
* @brief Divide o comando pelos espaços (' ').
* @param cmdline - comando todo digitado.
* @param argv - matriz de comandos.
* @param argcc - numero de argumentos.
* @return int - processo em background ou foreground
*/
int parser(const char *cmdline, char **argv, int *argcc)
{
	static char array[MAXLINE]; 
	char *buf = array;          
	char *delim;                
	int argc;                   
	int bg;                     

	strcpy(buf, cmdline);
	buf[strlen(buf)-1] = ' '; /**< Coloca espaço onde era \n. */
	while (*buf && (*buf == ' ')) 
		buf++;
	argc = 0;
	if (*buf == '\'') {
		buf++;
		delim = strchr(buf, '\'');
	}
	else {
		delim = strchr(buf, ' ');
	}

	while (delim) {
		argv[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ' '))
			buf++;

		if (*buf == '\'') {
			buf++;
			delim = strchr(buf, '\'');
		}
		else {
			delim = strchr(buf, ' ');
		}
	}
	argv[argc] = NULL;
	if (argc == 0)
		return 1;
	*argcc=argc;
	/**< Verifica se último argumento é & (background).*/
	if ((bg = (*argv[argc-1] == '&')) != 0) {
		argv[--argc] = NULL;
	}
	return bg;
}
/**
* @brief Processa em background.
* @param argv - matriz de comandos.
* @see Fork()
* @see Sigprocmask()
* @see Setpgid()
* @see Signal()
*/
void procBg(char *argv[])
{ 
	char job_name[32];
	int pid = -1;
	/**< Bloqueia novamente os sinais, com a mascara.*/
	Sigprocmask(SIG_BLOCK,&maskchld,NULL);
	/**< Dá fork.*/
	pid = Fork();

	if(pid == 0)  /**< Filho.*/
	{	
		/**< Muda grupo do processo.*/
		Setpgid(0,0);   
		/**< Desbloqueia sinais. */
		Sigprocmask(SIG_UNBLOCK,&maskchld,NULL); 
		/**< Religa sinais defaults.*/ 
		Signal(SIGINT, SIG_DFL);     
		Signal(SIGTSTP, SIG_DFL);    
		/**< Executa comando digitado.*/
		if(execvp(argv[0], argv) < 0){
			printf("%s: Comando nao encontrado!\n",argv[0]);
			exit(1);
		}
		return;
	}   
	else{		/**< Pai.*/
		strcpy(job_name,argv[0]);
		strcat(job_name,"\n");
		/**< Adiciona em BG.*/		
		adicionaJob(jobs,pid,BG,job_name);
		/**< Desbloqueia sinais.*/
		Sigprocmask(SIG_UNBLOCK,&maskchld,NULL);
	}
}
/**
* @brief Processa em foreground.
* @param argv - matriz de comandos.
* @see Fork()
* @see Sigprocmask()
* @see Setpgid()
* @see Signal()
* @warning no pai, um processo que roda no proprio shell recebe sinal parado, ex.:vi
*/
void procFg(char *argv[])
{
	int pid = -1;
	char* job_name;
	/**< Bloqueia novamente os sinais, com a mascara.*/
	Sigprocmask(SIG_BLOCK,&maskchld,NULL);
	/**< Dá fork.*/
	pid = Fork();

	if(pid == 0)	/**< Filho.*/
	{ 
		/**< Muda grupo do processo.*/
		Setpgid(0,0);
		/**< Religa sinais defaults.*/
		Signal(SIGINT,  SIG_DFL);   
		Signal(SIGTSTP, SIG_DFL);
		Signal(SIGCHLD, SIG_DFL);
		/**< Desbloqueia sinais. */
		Sigprocmask(SIG_UNBLOCK,&maskchld,NULL);
		/**< Controle volta ao shell.*/
		tcsetpgrp(shell_terminal, shell_pgid); 
		/**< Executa comando digitado.*/
		if(execvp(argv[0], argv) < 0){
			printf("%s: Comando nao encontrado!\n",argv[0]);
			exit(1);
		}
	}
	else{		/**< Pai.*/ /**< WARNING.*/
		job_name = strcat(argv[0],"\n");
		/**< Adiciona em FG.*/	
		adicionaJob(jobs,pid,FG,job_name);
		/**< Religa sinais de manipulação.*/		
		Signal(SIGTSTP, manipulaCtrlZ);        
		Signal(SIGINT,  manipulaCtrlC);
		Signal(SIGCHLD, manipulaChld);
		/**< Muda grupo do processo.*/		
		Setpgid(pid,pid);				
		tcsetpgrp(shell_terminal, pid);
		Sigprocmask(SIG_UNBLOCK,&maskchld,NULL);		
		/**< Pai espera filho terminar.*/
		esperaFg(pid);
		/**< Controle volta ao shell.*/		
		tcsetpgrp(shell_terminal, shell_pgid);		
	}
	return;
}
/**
* @brief Processa comandos com redirecionamento/pipe.
* @param cmdline - comando digitado.
* @param frompipe - marca ate onde o pipe vai
* @param ifd
* @param fdstd
* @see Sigemptyset()
* @see Sigaddset()
* @see Sigprocmask()
* @see esperaFg()
*/
void procDirPipe(char *cmdline, int frompipe, int ifd[2], int fdstd[2])
{
	char **argv;
	char for_cmdline[MAXLINE];
	char *after_cmdline;
	int i, pid, argcc;
	int topipe = 0; 
	int pfd[2]; 

	if( frompipe )
	{
		fdstd[0] = dup(fileno(stdin));
		dup2( ifd[0], 0);
	}
	/**< Verifica ate onde vai o pipe.*/
	if( (after_cmdline = strstr(cmdline, "|")) != NULL)
	{
		topipe = 1;
		strncpy( for_cmdline, cmdline, strlen(cmdline) - strlen(after_cmdline) );
		for_cmdline[strlen(cmdline) - strlen(after_cmdline)] = '\0';
		strcpy( after_cmdline, after_cmdline+1 );
		if( pipe( pfd ) == -1)
		{
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		fdstd[1] = dup(fileno(stdout));
		dup2(pfd[1], 1);
	}
	else
	{
		strcpy(for_cmdline, cmdline);
	}
	/**< Aloca a matriz de argumentos.*/
	argv = (char**) calloc( MAXARGS, MAXLINE );
	for( i = 0; i < MAXARGS; i++ )
		argv[i] = (char*) calloc( MAXLINE, sizeof(char));
	/**< Separa os argumentos.*/
	parser(for_cmdline, argv,&argcc);
	/**< Bloqueia os sinais usando mascara.*/
	sigset_t newmask; 

	Sigemptyset(&newmask); 
	Sigaddset(&newmask, SIGCHLD);
	Sigprocmask(SIG_BLOCK, &newmask, NULL); 
	/**< Dá fork.*/
	pid = fork();
	if ( pid == 0 ) /**< Filho.*/
	{ 
		Sigprocmask(SIG_UNBLOCK, &newmask, NULL); 
		
		Setpgid(0,0);

		if(execvp(argv[0], argv) < 0){
			printf("%s: Comando nao encontrado!\n",argv[0]);
			exit(1);
		}
	}
	/**< Pai.*/
	adicionaJob(jobs,pid,FG,cmdline);
	Sigprocmask(SIG_UNBLOCK, &newmask, NULL);

	esperaFg(pid);

	if( frompipe )
		dup2(fdstd[0],fileno(stdin));
	/**< Chamada recursiva para o resto do comando.*/
	if( topipe ){
		dup2(fdstd[1],fileno(stdout));
		close(pfd[1]);
		procDirPipe(after_cmdline, 1, pfd, fdstd);
	}
}
/**
* @brief Seta as variaveis de redirecionamento.
* @param cmdline - comando digitado.
*/
void setaDir(char *cmdline)
{
	char **argv;
	char file_in[50], file_out[50], file_app[50], file_err[50]; 
	char aux[1024] = {0};
	int i, argcc;
	/**< Aloca matriz de comandos.*/
	argv = (char**) calloc( MAXARGS, MAXLINE );
	for( i = 0; i < MAXARGS; i++ )
		argv[i] = (char*) calloc( MAXLINE, sizeof(char));

	parser(cmdline, argv,&argcc);

	for(i=0; i < MAXARGS; i++){
		if (argv[i]!=NULL) {  /**< Verifica os argumentos e se seta a variavel correspondente.*/
			if (strcmp(argv[i],"<") == 0){
				redir_in = 1;
				strncpy(file_in, argv[i+1], strlen(argv[i+1]) + 1);
				sprintf(argv[i], " ");
				sprintf(argv[i+1], " ");
			}
			if ( (strcmp(argv[i],">>") == 0) || (strcmp(argv[i],"1>>") == 0) ){
				redir_app = 1;
				strncpy(file_app, argv[i+1], strlen(argv[i+1]) + 1);
				sprintf(argv[i], " ");
				sprintf(argv[i+1], " ");
			}
			if ( (strcmp(argv[i],">") == 0) || (strcmp(argv[i],"1>") == 0) ){
				redir_out = 1;
				strncpy(file_out, argv[i+1], strlen(argv[i+1]) + 1);
				sprintf(argv[i], " ");
				sprintf(argv[i+1], " ");
			}

			if (strcmp(argv[i],"2>") == 0){
				redir_err = 1;
				strncpy(file_err, argv[i+1], strlen(argv[i+1]) + 1);
				sprintf(argv[i], " ");
				sprintf(argv[i+1], " ");
			}
			if (strcmp(argv[i],"2>>") == 0){
				redir_erra = 1;
				strncpy(file_err, argv[i+1], strlen(argv[i+1]) + 1);
				sprintf(argv[i], " ");
				sprintf(argv[i+1], " ");
			}
		}
	}

	/**< Fecha o stdin e abre do arquivo dado.*/
	if (redir_in == 1){
		fflush(stdin);
		fgetpos(stdin, &pos_in);
		fpin = dup(fileno(stdin)); /**< dup copia stdin para o fdin.*/
		freopen(file_in, "r", stdin); /**< read.*/
	}

	/**< Fecha o stdout e abre do arquivo dado.*/
	if (redir_out == 1){
		fflush(stdout);
		fgetpos(stdout, &pos_out);
		fpout = dup(fileno(stdout)); /**< dup copia stdout para o fpout.*/
		freopen(file_out, "w", stdout); /**< write.*/
	}
	/**< Fecha o stdout e abre do arquivo dado. */
	else if (redir_app == 1){
		fflush(stdout);
		fgetpos(stdout, &pos_out);
		fpout = dup(fileno(stdout)); /**< dup copia stdout para fpout.*/
		freopen(file_app, "a", stdout);  /**< append.*/
	}

	/**< Fecha o stderr e abre do arquivo dado. */
	if (redir_err == 1){
		fflush(stderr);
		fgetpos(stdout, &pos_out);
		fperr = dup(fileno(stderr)); /**< dup copia stderr para o fderr.*/
		freopen(file_err, "w", stderr); /**< write.*/
	}

	/**< Fecha o stderr e abre o arquivo dado.*/
	else if (redir_erra==1){
		fflush(stderr);
		fgetpos(stdout, &pos_out);
		fperr = dup(fileno(stderr)); /**< dup copia stderr para o fderr.*/
		freopen(file_err, "a", stderr); /**< append.*/
	}
	
	/**< Remonta a cmdline que foi quebrado no começo.*/
	for (i=0; i< MAXARGS; i++){
		if (argv[i]!=NULL) {
			strcat(aux,argv[i]);
			strcat(aux, " ");
		}
	}
	strcpy(cmdline, aux);
}
/**
* @brief Limpa as flags antes setadas.
*/
void limpaFlags()
{
	/**< Reseta STDERR.*/
	if (redir_err || redir_erra){
		fflush(stderr);
		dup2(fperr, fileno(stderr));
		close(fperr);
		clearerr(stderr);
		fsetpos(stderr, &pos_in);
	}
	/**< Reseta STDOUT.*/
	if (redir_out || redir_app) {
		fflush(stdout);			/**< limpou o que tinha no arquivo.*/
		dup2(fpout, fileno(stdout));	/**< voltou o stdout para o original dele.*/
		close (fpout);			/**< fechou o ponteiro temporario.*/
		clearerr (stdout);		/**< limpou o erro (caso precise).*/
		fsetpos (stdout, &pos_out);	/**< voltou para a posicao que estava antes.*/
	}
	/**< Reseta STDIN.*/
	if (redir_in) {
		fflush(stdin);
		dup2(fpin, fileno(stdin));
		close(fpin);
		clearerr(stdin);
		fsetpos(stdin, &pos_in);
	}

	/**< Reseta as flags. */
	redir_out=0;
	redir_in=0;
	redir_app=0;
	redir_err=0;
	redir_erra=0;
}
