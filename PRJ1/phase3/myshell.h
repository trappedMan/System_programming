//SP PROJECT1 : PHASE2
//20181650
#define MAXARG 128
#define NONE 0
#define MAXCOM 10
#include <errno.h>
void eval(char* com_line);
static void sigint_handler(int sig);
static void sigtstp_handler(int sig);
void exec_pipe(char** comlist,char** argv,int index,int num_com,int* fd,int bgflag);
void wait_proc(int num,int pid);
int builtin_command(char** argv);
void exec_com(char **argv);
void devideCommand(char* buff,char** comlist,int* num_com); 
int parseLine(char* command,char** argv);
