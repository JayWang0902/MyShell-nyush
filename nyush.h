#ifndef NYUSH_H
#define NYUSH_H

#include <stdlib.h>
#include <stdio.h>

typedef struct{
    pid_t pid;
    char command[1000];
}job;

extern job job_list[1000];
extern int job_count;

void prompt();
void ignore_signals();
void resume_signals();
char** parse_line(char* command);
char* concat_list(char** command_list);
int my_system(char** command_list);
int buildin_cd(char** command_list);
int buildin_jobs(char** command_list);
int buildin_fg(char** command_list);
int buildin_exit(char** command_list);
char** complete_command(char** command_list);
char*** parse_pipe(char** command_list, int pipe_count);
int io_redirection(char** command_list, int pipe_count, int pipe_i);
//int exec_pipe(char*** pipe_list, int pipe_count, int pipe_i);
int exec_pipe(char*** pipe_list, int pipe_count);
int executor(char** command_list);

#endif

