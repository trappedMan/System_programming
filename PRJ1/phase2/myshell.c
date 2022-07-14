#include "myshell.h"
#include "csapp.h"

char com[30]; //changing command for "/bin/[command]"
char com2[30];	//changing command for "/usr/bin/[command]"
int main() 
{
	signal(SIGINT,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
    char com_line[MAXLINE]; // command line input by stdin
	//reading loop until the shell program terminates
    while (1) {
		fputs("CSE4100:myshell > ",stdout);                   
		fgets(com_line, MAXLINE, stdin); 
		eval(com_line);	//evaluate command line
    } 
}
  
//function evaluating command line 
void eval(char *com_line) 
{
    char buff[MAXLINE];   //command line
	char *comlist[MAXCOM];	//command list divided by '|'
    char *argv[MAXARG]; 	//args list after parsing a command line
    int bgflag,num_com=0;
	int fd[2]; 
    
    strcpy(buff, com_line);
	devideCommand(buff,comlist,&num_com);	//devide command by '|', and store the pieces in comlist

	argv[0]=NULL;
	bgflag=parseLine(comlist[0],argv);
	if(!argv[0]||builtin_command(argv))		//blank line or builtin command -> return
		return;
	exec_pipe(comlist,argv,1,num_com,fd);	//recursive function-pipe
}
void exec_pipe(char** comlist, char** argv, int index,int num_com,int* fd){
	int status,bgflag=0;
	pid_t pid;
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
	if(!bgflag){
			//close old-fasioned descriptor
			if(index>1){
				close(fd[0]);close(fd[1]);
			}
			//close all descriptor
			if(index==num_com)
				for(int i=3;i<32;i++)
					close(i);
			//wait for child process
			wait(&status);
	}
	if(index<num_com){
		bgflag = parseLine(comlist[index], argv);
		exec_pipe(comlist,argv,index+1,num_com,ffd);	//call this function recursively
	}
}

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
	exit(0);
}

int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "exit")) // exit command
		exit(0);  

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
    int argc=0,bgflag=0;

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

