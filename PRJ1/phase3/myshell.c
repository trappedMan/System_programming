#include "myshell.h"
#include "csapp.h"

char com[30]; //changing command for "/bin/[command]"
char com2[30];	//changing command for "/usr/bin/[command]"
char bgc[2];	//dummy character for erasing '&' from command line
char temp_buf[MAXLINE];	//store command line
typedef struct _job{
	int num;	//job number
	pid_t pid;	//job pid
	char state;	//R for Running, S for suspended
	char com[MAXLINE];	//command
}jobtype;

jobtype jobs[300];	//store the background jobs
pid_t c_procs[300];	//waiting process for background jobs
pid_t cur_pid=0;	//current pid: target for signal handler
int jobcount=1;	//indicate number of elements for array 'jobs'
int flag=0;	//background jobs vs foreground jobs suspended 

int main() 
{
	signal(SIGINT,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
    char com_line[MAXLINE]; // command line input by stdin
	bgc[0]='&';bgc[1]='\0';
	//initialize the job list
	for(int i=1;i<300;i++){
		jobs[i].num=0;
		c_procs[i]=0;
	}
	//reading loop until the shell program terminates
    while (1) {
		fputs("CSE4100:myshell > ",stdout);                   
		fgets(com_line, MAXLINE, stdin); 
		eval(com_line);	//evaluate command line
    } 
}

static void sigint_handler(int sig){
	//kill cur_pid process
	if(cur_pid){
		kill(cur_pid,SIGKILL);
		fprintf(stdout,"\n");
	}
	//if killed, delete it from job list.(By jobs[i].num=0, built-in comand 'jobs' will not show this process later) 
	for(int i=0;i<jobcount;i++){
		if(jobs[i].pid==cur_pid){
			jobs[i].num=0;
			break;
		}
	}
}
static void sigtstp_handler(int sig){
	//stop cur_pid process
	if(cur_pid){
		kill(cur_pid,SIGSTOP);
		fprintf(stdout,"\n");
	}
	//if waiting process exists, stop it too.
	if(flag){
		for(int i=0;i<jobcount;i++){
			if(jobs[i].pid==cur_pid){
				kill(c_procs[i],SIGSTOP);
				break;
			}
		}
	}
}
//function evaluating command line 
void eval(char *com_line) 
{
    char buff[MAXLINE];   //command line
	char *comlist[MAXCOM];	//command list divided by '|'
    char *argv[MAXARG]; 	//args list after parsing a command line
    int bgflag,status,num_com=0;	//bgflag: background? //num_com: number of command for pipe
	int fd[2];	//empty array
	pid_t pid;
    
	//parent process call wait() periodically in order to reap background child process
	for(int i=1;i<jobcount;i++){
		if(waitpid(c_procs[i],&status,WNOHANG)>0){
			jobs[i].num=0;
		}
	}
    strcpy(buff, com_line);
	//if command line includes '&', it is treated as background process
	if(strchr(buff,'&'))
		bgflag=1;
	else
		bgflag=0;

	devideCommand(buff,comlist,&num_com);	//devide command by '|', and store the pieces in comlist

	argv[0]=NULL;
	parseLine(comlist[0],argv);
	if(!argv[0]||builtin_command(argv))		//blank line or builtin command -> return
		return;

	strcpy(temp_buf,"\0");	
	int j=0;
	//store argv list except '&'
	while(argv[j]){
		strcat(temp_buf," ");
		strcat(temp_buf,argv[j++]);
	}

	/*When background process starts, parent process calls fork() and process move forward in child process,
	and parent process return to main func for reading another command line for stdin.
	This is why one more process is made for waiting background job terminate.*/
	if(bgflag){
		if((pid=fork())>0){	//parent process(add background process in job list)
			jobs[jobcount].num=jobcount;
			jobs[jobcount].pid=(int)pid+num_com;
			jobs[jobcount].state='R';
			c_procs[jobcount]=pid;
			strcpy(jobs[jobcount].com,temp_buf);
			fprintf(stdout,"[%d]  %d %s\n",jobs[jobcount].num,jobs[jobcount].pid,jobs[jobcount].com);
			jobcount++;
			return;
		}
		else{
			exec_pipe(comlist,argv,1,num_com,fd,1);	//recursive function-pipe
			waitpid(getpid()+num_com,&status,0);	//by adding this line, we can catch SIGCHLD which was sent by terminated child process
			exit(0);
		}
	}
	//foreground jobs move forward in parent process.
	else{
		exec_pipe(comlist,argv,1,num_com,fd,0);	//recursive function-pipe
		//cur_pid=getpid();
	}
}
void exec_pipe(char** comlist, char** argv, int index,int num_com,int* fd,int bgflag){
	int i,status,flag=0;
	pid_t pid,tmp;
	int ffd[2];	//file descriptor list for piping
	pipe(ffd);	//pipe
	pid=fork();
	if(pid==0){	//child process
		//command don't include pipe character '|'
		if(num_com==1){
			close(ffd[0]);close(ffd[1]);
		}
		
		//pipe-last command
		if(num_com!=1&&index==num_com){
			dup2(fd[0],0);
			close(fd[1]);close(fd[0]);
			close(ffd[1]);close(ffd[0]);
		}
		//pipe-first command
		else if(num_com!=1&&index==1){
			dup2(ffd[1],1);
			close(ffd[0]);close(ffd[1]);
		}
		//pipe-middle command
		else if(num_com!=1){
			dup2(fd[0],0);
			close(fd[0]);
			close(fd[1]);
			close(ffd[0]);
			dup2(ffd[1],1);
			close(ffd[1]);
		}
		
		//executing command
		exec_com(argv);
	}
	//close old-fasioned descriptor
	if(index>1){
		close(fd[0]);close(fd[1]);
	}
	//close all descriptor
	if(index==num_com)
		for(int i=3;i<32;i++)
			close(i);
	//wait for child process terminate
	if(!bgflag){
		wait_proc(0,pid);
	}
	if(index<num_com){
		parseLine(comlist[index], argv);
		exec_pipe(comlist,argv,index+1,num_com,ffd,bgflag);	//call this function recursively
	}
}


void wait_proc(int num,int pid){
		int i,status;
		signal(SIGINT,sigint_handler);
		signal(SIGTSTP,sigtstp_handler);
		cur_pid=pid;
		if(!flag){	//not having waiting process, directly wait child process
			waitpid(pid,&status,WUNTRACED);
		}
		else{	//having waiting process, indirectly wait waiting process
			waitpid(c_procs[num],&status,WUNTRACED);
		}
		signal(SIGINT,SIG_IGN);
		signal(SIGTSTP,SIG_IGN);
		if(WIFSTOPPED(status)){	//getting SIGTSTP signal, change job state.
			for(i=1;i<jobcount;i++){
				if(jobs[i].num&&jobs[i].pid==pid){
					jobs[i].state='S';
					break;
				}
			}
			//if this job isn't in job list, add it
			if(i==jobcount){
				jobs[jobcount].num=jobcount;
				jobs[jobcount].pid=pid;
				jobs[jobcount].state='S';
				strcpy(jobs[jobcount].com,temp_buf);
				c_procs[jobcount]=pid;
				jobcount++;
			}
		}
		else if(WIFEXITED(status)){	//termination
			cur_pid=0;
		}
}

//execve() with /usr/bin or /bin or argv[0] itself
void exec_com(char** argv){	
	strcpy(com,"/bin/");
	strcat(com,argv[0]);
	argv[0]=com;
		if (execve(argv[0], argv, environ) < 0) {
		strcpy(com2,"/usr");
		strcat(com2,com);
		argv[0]=com2;
		if(execve(argv[0],argv,environ)<0){
			argv[0]+=9;
			if(execve(argv[0],argv,environ)<0){
				printf("%s: command not found.\n", argv[0]);
   				exit(0);
			}
		}
	}
}

int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "exit")) // exit command
		exit(0);  
	else if(!strcmp(argv[0],"jobs")){	//jobs command- show all existing background jobs
		for(int i=1;i<jobcount;i++){
			if(jobs[i].num>0){	//if num==0, that is terminated job
				if(jobs[i].state=='S')
					fprintf(stdout,"[%d] Suspended %s\n",jobs[i].num,jobs[i].com);
				else if(jobs[i].state=='R')
					fprintf(stdout,"[%d] Running %s\n",jobs[i].num,jobs[i].com);
			}
		}
		return 1;
	}
	else if(!strcmp(argv[0],"kill")){
		if(!argv[1]){
			fprintf(stdout,"Give me argument!\n");
			return 1;
		}
		argv[1]++;	//ignore '%'
		int tmp=atoi(argv[1]);
		if(!jobs[tmp].num){
			fprintf(stdout,"No Such Job\n");
			return 1;
		}
		//if suspended job, continue it and kill it.
		if(jobs[tmp].state=='S'){
			kill(jobs[tmp].pid,SIGCONT);
			kill(c_procs[tmp],SIGCONT);
		}
		kill(jobs[tmp].pid,SIGKILL);
		jobs[tmp].num=0;
		return 1;
	}
	else if(!strcmp(argv[0],"fg")){	
		if(!argv[1]){
			fprintf(stdout,"Give me argument!\n");
			return 1;
		}
		argv[1]++;	//ignore '%'
		int tmp=atoi(argv[1]);
		if(jobs[tmp].num){
			//if suspended job, continue it for foreground jobs and wait.
			if(c_procs[tmp]==jobs[tmp].pid){	//not having waiting process(started in foreground initially but later suspended
				if(jobs[tmp].state=='S'){
					kill(jobs[tmp].pid,SIGCONT);
				}
			}
			else{
				if(jobs[tmp].state=='S'){
					kill(jobs[tmp].pid,SIGCONT);
					kill(c_procs[tmp],SIGCONT);
				}
				flag=1;
			}
			wait_proc(tmp,jobs[tmp].pid);
			flag=0;
		}
		else{
			fprintf(stdout,"No Such Job\n");
		}
		return 1;
	}
	else if(!strcmp(argv[0],"bg")){
		if(!argv[1]){
			fprintf(stdout,"Give me argument!\n");
			return 1;
		}
		argv[1]++;	//ignore '%'
		int tmp=atoi(argv[1]);
		if(jobs[tmp].num){
			//bg command apply only to suspended job
			if(jobs[tmp].state=='R'){
				fprintf(stdout,"Now Running!\n");
			}
			else{
				//continue it in background
				if(c_procs[tmp]==jobs[tmp].pid){	////not having waiting process(started in foreground initially but later suspended
					kill(jobs[tmp].pid,SIGCONT);
					jobs[tmp].state='R';
				}
				else{
					kill(jobs[tmp].pid,SIGCONT);
					kill(c_procs[tmp],SIGCONT);
					jobs[tmp].state='R';
				}
			}
		}
		else{
			fprintf(stdout,"No Such Job\n");
		}
		return 1;
	}
	else if (!strcmp(argv[0],"cd")){		//cd command in bulit-in command
		if(chdir(argv[1])<0){
			fputs("No Such Directory!\n",stdout);
		}
		return 1;
	}
	else
    	return 0;
}

//devide command by '|' delimeter and store each command in comlist
void devideCommand(char* buff,char** comlist,int* num_com){
	char *deli;		//'|'->delimeter
	buff[strlen(buff)-1]='|';
	while((deli=strchr(buff,'|'))){
		comlist[*num_com]=buff;
		*deli='\0';
		buff=deli+1;
		++(*num_com);
	}
	comlist[*num_com]=NULL;
}
//parsing: divide command line into tokens stored in argv
int parseLine(char *command, char **argv) 
{
    char *deli;	//space->delimeter
    char *delibg;
	int argc=0,bgflag=0;

    //command[strlen(command)] = ' ';  //For using ' ' as delimeter, make last character '\0'->' '
    while (*command && (*command == ' ')) // ignore leading spaces
		command++;
    while (1) {
		if(*command=='\''){
			command++;
			deli=strchr(command,'\'');
		}
		else if(*command=='\"'){
			command++;
			deli=strchr(command,'\"');
		}
		else if(!(deli = strchr(command,' ')))
			break;
		argv[argc] = command;
		*deli = '\0';	//' '->'\0'. each argv stores the address next to this '\0'
		command = deli + 1;
		while (*command && (*command == ' ')) //ignore leading spaces
            command++;
		++argc;
    }
	if(strlen(command)){
		argv[argc++]=command;
		command++;
	}
	if((delibg=strchr(command,'&'))){
		*delibg='\0';
		argv[argc++]=bgc;
	}
    if (argc == 0)  //ignore blank command line
		return NONE;
	argv[argc]= NULL;

    if (*(argv[argc-1]) == '&'){
		bgflag=1;
		argv[--argc] = NULL;
	}
    return bgflag;
}

