#define NONE 0
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
    char buff[MAXLINE];   //copy command line
    char *argv[MAXARG]; 	//args list after parsing a command line
    int chld_proc,bgflag;
    pid_t pid; 
    
    strcpy(buff, com_line);
	argv[0]=NULL;
    bgflag = parseLine(buff, argv); //parsing command line(return bgflag)
    if (argv[0] == NULL)  
		return;  

    if (!builtin_command(argv)) { //if argv[0] is built-in command, pass
    	strcpy(com,"/bin/");
		strcat(com,argv[0]);
		argv[0]=com;
		if((pid=fork())==0){
			// /usr/bin/[command] or /bin/[command]
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
	}
	//parent process waits for child process terminated
	if (!bgflag){ 
		wait(&chld_proc);
	}
	return;
}

int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "exit")) // exit command
		exit(0);  

	else if (!strcmp(argv[0],"cd")){//cd command in bulit-in command
		if(chdir(argv[1])<0){
			fputs("No Such Directory!\n",stdout);
		}
		return 1;
	}
	else
    	return 0;
}

//parsing: divide command line into tokensi and store in argv
int parseLine(char *buff, char **argv) 
{
    char *deli;	//space->delimeter
    int argc=0,bgflag=0;

    buff[strlen(buff)-1] = ' ';  //For using ' ' as delimeter, make last character '\n'->' '
    while (*buff && (*buff == ' ')) // ignore leading spaces
		buff++;
    while (1) {
		if(*buff=='\''){	//arg with small quotes
			buff++;
			deli=strchr(buff,'\'');
		}
		else if(*buff=='\"'){	//arg with big quotes
			buff++;
			deli=strchr(buff,'\"');
		}
		else if(!(deli = strchr(buff,' ')))
			break;
		argv[argc] = buff;
		*deli = '\0';	//' '->'\0'. each argv stores the address next to this '\0'
		buff = deli + 1;
		while (*buff && (*buff == ' ')) //ignore leading spaces
           	buff++;
		++argc;
    }
    if (argc == 0)  //ignore blank command line
		return NONE;
	argv[argc]= NULL;

    return bgflag;
}

