// SP PROJECT1 : PHASE 1
// 20181650

#define MAXARG 128
#include <errno.h>

void eval(char* cmdline);
int builtin_command(char** argv);
int parseLine(char* buf,char** argv);
