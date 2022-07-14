//SP PROJECT1 : PHASE2
//20181650
#define MAXARG 128
#define NONE 0
#define MAXCOM 10
#include <errno.h>
void eval(char* com_line);
void exec_pipe(char** comlist,char** argv,int index,int num_com,int* fd);
int builtin_command(char** argv);
void exec_com(char **argv);
void devideCommand(char* buff,char** comlist,int* num_com); 
int parseLine(char* command,char** argv);
