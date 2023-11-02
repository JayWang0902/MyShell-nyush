#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include "nyush.h"

job job_list[1000];
int job_count=0;

void prompt(){
    int size = 1000;
    char buf[1000];
    char* bn;
    if (getcwd(buf, (size_t)size)==NULL){
        perror("getcwd error");
        exit(1);
    } //https://pubs.opengroup.org/onlinepubs/007904975/functions/getcwd.html
    bn = basename(buf);  //https://www.ibm.com/docs/en/zos/2.1.0?topic=functions-basename-return-last-component-path-name
    printf("[nyush %s]$ ",bn);
    fflush(stdout);
}

void ignore_signals(){
    signal(SIGINT, SIG_IGN);   
    signal(SIGQUIT, SIG_IGN);  
    signal(SIGTSTP, SIG_IGN);
}

void resume_signals(){
    signal(SIGINT, SIG_DFL);   
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
}

char** parse_line(char* command){
    if (strcmp(command, "\n") == 0 || strcmp(command, " ")==0) { // Check if the command is just a newline character
        //printf("just empty\n");
        return NULL; // Return NULL for an empty line
    }
    if (strlen(command)==0){ // blank line, no command
        //printf("nothing here\n");
        return NULL;
    }
    int init_size = 1;
    char** tokens = malloc(init_size*sizeof(char*));
    char* token;
    char* saveptr;
    token= strtok_r(command," ", &saveptr);
    while(token != NULL){
        token[strcspn(token, "\n")] = 0; // remove "\n"
        //token[strcspn(token, " ")] = 0;
        tokens[init_size-1] = token;
        //printf("token is %s\n", token);
        init_size ++;
        tokens = realloc(tokens, init_size*sizeof(char*));
        if (tokens == NULL) {
            //perror("realloc failed");
            exit(EXIT_FAILURE);
        }
        token = strtok_r(NULL," ", &saveptr);
            
    }
    tokens[init_size-1] = NULL;
    return tokens;
}

char* concat_list(char** command_list){
    char* command = malloc(1000*sizeof(char));
    for(int i=0;command_list[i]!=NULL;i++){
        strcat(command,command_list[i]);
        strcat(command," ");
    }
    return command;
}

int my_system(char** command_list){ 
    //printf("Running in system\n");
    char* program = command_list[0];
    pid_t pid = fork();
    char** result = complete_command(command_list); // the command before any redirection
    if (pid == 0){   // if it's child
        // printf("The program to run is: %s\n",program);
        // fflush(stdout);
        char path[1000];
        snprintf(path, sizeof(path), "/usr/bin/%s", program);
        // printf("The program to run is: %s\n",program);
        // printf("Got the path:%s\n",path);
        //fflush(stdout);
        struct stat buf;
        if(strchr(program, '/') == NULL&&stat(path, &buf)!=0){
            fprintf(stderr,"Error: invalid program\n");
            exit(-1);
        }
        //printf("It's executable\n");
        resume_signals();
        //printf("Signal resumed\n");
        execvp(program,result);
        
        //execl("/bin/sh","/bin/sh","-c",command,NULL);
        fprintf(stderr,"Error: invalid program\n");
        exit(-1);
    }
    else{

        int status = 0;
        pid_t wpid;
        while ((wpid = waitpid(pid, &status, WUNTRACED)) > 0) {
            // if it's stopped put in the list
            if (WIFSTOPPED(status)) {
                //printf("Process stopped\n");
                char* command = concat_list(command_list);
                //printf("%s",command);
                strcpy(job_list[job_count].command,command);
                job_list[job_count].pid = wpid;
                job_count++;
                free(command);
                break;
            }
            if (WIFEXITED(status)){
                //printf("Process terminated\n");
                break;
            }
        }
        //printf("There're %d jobs.\n",job_count);
        for(int i=0;result[i]!=NULL;i++){
                free(result[i]);
        }
        free(result);
        return 0;
    }
    
}


int buildin_cd(char** command_list){
    if (command_list[1]== NULL || command_list[2]!=NULL){
        fprintf(stderr,"Error: invalid command\n");
        return 1;
    }
    else if (chdir(command_list[1])!=0){
        fprintf(stderr,"Error: invalid directory\n");
        return 1;
    }
    return 0;
}

int buildin_jobs(char** command_list){
    // while(pid = waitpid(-1, &status, WUNTRACED)>0){
    //      if(WIFSTOPPED(status)==0){
    //         sus_jobs[pos] = pid;
    //      }
    // }
    if(command_list[1]!=NULL){
        fprintf(stderr,"Error: invalid command\n");
        return 1;
    }
    if(job_count!=0){
        //printf("I found some jobs!\n");
        for (int i = 0; i < job_count; i++) {
                int l = strlen(job_list[i].command);
                while(isspace(job_list[i].command[l - 1])){
                    --l;
                }
                char* trimmed=malloc(1000*sizeof(char));
                trimmed = strndup(job_list[i].command, l);
                printf("[%d] %s\n",i+1,trimmed);
                free(trimmed);
            }
                
        }
    return 0;
}
int buildin_fg(char** command_list){
    if(command_list[1]==NULL || command_list[2]!=NULL){
        fprintf(stderr,"Error: invalid command\n");
        return 1;
    }
    int status;
    int index;
    pid_t pid_resume;
    index = atoi(command_list[1]);
    //printf("%d",index);
    //printf("%d",job_count);
    if (index>job_count){
        fprintf(stderr,"Error: invalid job\n");
        return 1;
    }
    pid_resume = job_list[index-1].pid;
    //printf("Command to resume:%s",job_list[index-1].command);
    //int i=index-1;
    char* command = strdup(job_list[index-1].command); // store the command of the job being removed
    //printf("The command removed is:%s\n",command);
    for (; index<job_count;index++){
        job_list[index-1] = job_list[index];
    }
    job_count--;

    kill(pid_resume,SIGCONT);
    //printf("Signal sent\n");
    waitpid(pid_resume, &status, WUNTRACED);
    // job_list[i].command = "\0"; //terminate
    // job_list[i].pid = -1;
    if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTSTP) {
        //printf("Child was stopped. Adding back to job list.\n");
        job_list[job_count].pid = pid_resume;
        strcpy(job_list[job_count].command,command); 
        job_count++;
    }
    free(command);
    return 0;
}
int buildin_exit(char** command_list){
    //printf("I'm in\n");
    if (command_list[1]!=NULL){
        fprintf(stderr,"Error: invalid command\n");
        return 1;
    }
    if(job_count!=0){
        fprintf(stderr,"Error: there are suspended jobs\n");
        return 1;
    }
    //printf("Nothing wrong, I can exit now.\n");
    exit(0); //exit the shell with successful exit status
}

char** complete_command(char** command_list){
    char** command = malloc(1000*sizeof(char*));
    int i = 0;
    for (; command_list[i] != NULL; i++) {
        if (strcmp(command_list[i], "<") == 0 || strcmp(command_list[i], ">") == 0 || 
        strcmp(command_list[i], ">>") == 0 || strcmp(command_list[i], "<<") == 0) {
            break; // Break the loop if a redirection sign is found
        } 
        else {
            command[i] = malloc(strlen(command_list[i]) + 1);
            strcpy(command[i],command_list[i]);
            //printf("Copying: %s\n",command[i]);
        }
    }
    command[i]=NULL; // terminate with NULL
    //printf("commanddddd is %s",command);
    return command;
}

char*** parse_pipe(char** command_list, int pipe_count){
    char*** pipe_list = malloc((pipe_count+1) * sizeof(char**));;
    int walk = 0;
    for(int c=0;c<=pipe_count;c++){
        //printf("Parsing %d pipe.\n",c);
        int sub_i = 0;
        char** sublist = malloc(1000*sizeof(char*));
        for(; command_list[walk] != NULL&&strcmp("|", command_list[walk])!=0;walk++,sub_i++){
            sublist[sub_i] = strdup(command_list[walk]);
            //printf("%s ", sublist[sub_i]);
        }
        sublist[walk]=NULL; //terminate list
        //printf("\n");
        pipe_list[c] = sublist;
        walk++;
        //free(sublist);
    }
    return pipe_list;
}

int io_redirection(char** command_list, int pipe_count, int pipe_i){
    if (command_list==NULL){
        fprintf(stderr,"Error: invalid command\n");
        return 1;
    }
    int fd_in, fd_out;
    int in_flag = 0, out_flag = 0, stdin_flag = 0;
    int status = 0;
    //FILE* fd_stdin;

    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);
    int text_num = 0; //number of tokens
    for (int i=0; command_list[i] != NULL; i++) {
        // printf("Processing element1: %s\n", command_copy[i]);
        if (strcmp(command_list[i], "<") == 0) {
            //printf("Here\n");
            if (command_list[i + 1] == NULL) {
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            fd_in = open(command_list[i + 1], O_RDONLY);
            if(fd_in==-1){
                fprintf(stderr,"Error: invalid file\n");
                return 1;
            }
            dup2(fd_in, STDIN_FILENO); // stdin
            close(fd_in);
            //printf("Input redirected\n");
            in_flag += 1;
        }
        else if (strcmp(command_list[i], "<<") == 0) {
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        else if (strcmp(command_list[i], ">") == 0) {
            if (command_list[i + 1] == NULL) {
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            //command_copy[i] = NULL; // Remove the '>' from the command
            fd_out = open(command_list[i + 1], O_WRONLY|O_CREAT|O_TRUNC, 0666);
            if(fd_out==-1){
                fprintf(stderr,"Error: invalid file\n");
                return 1;
            }
             //https://stackoverflow.com/questions/22697414/appending-to-a-file-with-linux-system-call
            dup2(fd_out, STDOUT_FILENO); // stdout
            close(fd_out);
            //printf("Output redirected\n");
            out_flag += 1;
        }
        else if (strcmp(command_list[i], ">>") == 0) {
            //printf("Here\n");
            if (command_list[i + 1] == NULL) {
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            //command_copy[i] = NULL; // Remove the '<' from the command
            fd_out = open(command_list[i + 1], O_WRONLY|O_CREAT|O_APPEND, 0666);
            if(fd_out==-1){
                fprintf(stderr,"Error: invalid file\n");
                return 1;
            }
            dup2(fd_out, STDOUT_FILENO); // stdout
            close(fd_out);
            //printf("Output redirected\n");
            out_flag += 1;
        }
        else if(strcmp(command_list[i], "-") == 0){
            // fd_stdin = stdin;
            // fd_in = fileno(fd_stdin);
            // dup2(fd_in, STDOUT_FILENO);
            // close(fd_in);
            stdin_flag+=1;

        }
        else{
            text_num++;
        }
    }

    //printf("NOT Valid command.\n");
    if(status==0){
        // printf("number of symbol: %d",in_flag+out_flag+1+stdin_flag);
        // printf("number of text: %d",text_num);
        if(out_flag==2 || in_flag==2){
            fprintf(stderr,"Error: invalid command\n");
            status=1; //don't return
        }
        else if(in_flag!=0&&out_flag!=0&&text_num!=(in_flag+out_flag+stdin_flag+1)){ // i: number of tokens
            fprintf(stderr,"Error: invalid command\n");
            status = 1;
        }
        if(in_flag){
            if(pipe_i!=0){
                fprintf(stderr,"Error: invalid command\n");
                status = 1;
            }
        }
        if(out_flag){
            if(pipe_i!=pipe_count){
                fprintf(stderr,"Error: invalid command\n");
                status = 1;
            }
        }
        if(status==0){
            if(pipe_count==0){ //no pipes in the command
                if(my_system(command_list)!=0){
                    return 1;
                }
            }
            else{ // it's running in pipes
                char* program = command_list[0];
                char* command[1000];
                int i = 0;
                for (; command_list[i] != NULL; i++) {
                    if (strcmp(command_list[i], "<") == 0 || strcmp(command_list[i], ">") == 0 || 
                    strcmp(command_list[i], ">>") == 0 || strcmp(command_list[i], "<<") == 0) {
                        break; // Break the loop if a redirection sign is found
                    }
                    else {
                        command[i] = malloc(strlen(command_list[i]) + 1);
                        strcpy(command[i],command_list[i]);
                        //printf("Copying: %s\n",command[i]);
                    }
                }
                command[i]=NULL;
                //printf("Execute the program.\n");
                resume_signals();
                execvp(program,command);
                fprintf(stderr,"Error: invalid program");
                exit(-1);

            }
        }
    }
    

    // restore STDIN and STDOUT
    if (in_flag) {
        dup2(original_stdin, STDIN_FILENO);
        close(original_stdin);
    }
    if (out_flag) {
        dup2(original_stdout, STDOUT_FILENO);
        close(original_stdout);
    }
    return status;
}

// int exec_pipe(char*** pipe_list, int pipe_count, int pipe_i){
//     //int status=0;
//     int p[2];
//     // int original_stdin;
//     // int original_stdout;
//     // if(pipe_i==0){
//     //     int original_stdin = dup(STDIN_FILENO);
//     //     int original_stdout = dup(STDOUT_FILENO);
//     // }
//     if(pipe_i>pipe_count){
//         return 0;
//     }
//     if(pipe_i==pipe_count){
//         io_redirection(pipe_list[pipe_i], pipe_count, pipe_i);
//         return 0;
//         //exit(0);
//     }
//     // if(pipe_i==pipe_count){ // it's the last pipe
//     //     // printf("Here's the last pipe\n");
//     //     // printf("Command is:\n");
//     //     // for(int i=0;pipe_list[pipe_i][i]!=NULL;i++){
//     //     //     printf("%s\n",pipe_list[pipe_i][i]);
//     //     // }
//     //     close(p[1]);
//     //     dup2(p[0],0);
//     //     close(p[0]);
//     //     io_redirection(pipe_list[pipe_i], pipe_count, pipe_i);
//     // }
//     if(pipe(p)==-1){
//         fprintf(stderr,"Pipe failed.\n");
//     }
//     pid_t pid = fork();
//     if(pid == 0){ //child process
//         close(p[1]); 
//         dup2(p[0],0);
//         close(p[0]);
//         exec_pipe(pipe_list,pipe_count,pipe_i+1);
//         exit(1);
//     }
//     else{
//         //printf("Here's pipe %d\n",pipe_i);
//         // printf("Command is:\n");
//         // for(int i=0;pipe_list[pipe_i][i]!=NULL;i++){
//         //     printf("%s\n",pipe_list[pipe_i][i]);
//         // }
//         close(p[0]);
//         dup2(p[1],1);
//         close(p[1]);
//         //printf("Run IO\n");
//         io_redirection(pipe_list[pipe_i], pipe_count, pipe_i);
//         //printf("exit\n");
//         waitpid(-1, 0, 0);
//         //printf("Got a child\n");
        
//     }
//     // dup2(original_stdin, STDIN_FILENO);
//     // close(original_stdin);
//     // dup2(original_stdout, STDOUT_FILENO);
//     // close(original_stdout);
//     // return(status);
//     return 0;
// }

int exec_pipe(char*** pipe_list, int pipe_count) {
    int p[2], pipe_end = -1;

    for (int i = 0; i <= pipe_count; i++) {
        // Create a pipe for every iteration except last one
        if (i != pipe_count){
            pipe(p);
        }

        pid_t pid = fork();
        if (pid < 0) {
            return 1;
        } else if (pid == 0) {  // child
            if (i != 0) {
                dup2(pipe_end, 0);
                close(pipe_end);
            }

            // If not the last command, write to end of the current pipe
            if (i != pipe_count) {
                dup2(p[1], 1);
                close(p[1]);
                close(p[0]); 
            }

            io_redirection(pipe_list[i], pipe_count, i);
            //perror("exec");
            exit(-1);
        } else {  // parent
            if (i != pipe_count) {
                close(p[1]);
            }

            if (pipe_end != -1) {
                close(pipe_end);
            }

            pipe_end = p[0];
        }
        //waitpid(-1, 0, 0);
    }
    while(waitpid(-1, 0, 0)>0){}
    

    return 0;
}

int executor(char** command_list){
    //printf("I'm in executor");
    //printf("Enter executor\n");
    if (command_list==NULL){
        // printf("Command is NULL");
        return 1;
    }
    //printf("Command not NULL");
    char* buildin_list[] = {"cd", "jobs", "fg", "exit"};
    int buildin_num = sizeof(buildin_list)/sizeof(buildin_list[0]);
    int (*buildin_func[])(char**) = {&buildin_cd,&buildin_jobs,&buildin_fg,&buildin_exit};
    char* program;
    program = command_list[0];
    if(strcmp("|",program)==0||strcmp("<",program)==0||strcmp(">",program)==0||strcmp(">>",program)==0||strcmp("-",program)==0){
        fprintf(stderr,"Error: invalid command\n");
        return 1;
    }
    int command_length=0;
    for(int i=0; command_list[i]!=NULL;i++){
        command_length+=1;
    }
    if(strcmp("|",command_list[command_length-1])==0||strcmp("<",command_list[command_length-1])==0||strcmp(">",command_list[command_length-1])==0||strcmp(">>",command_list[command_length-1])==0||strcmp("-",command_list[command_length-1])==0){
        fprintf(stderr,"Error: invalid command\n");
        return 1;
    }
    
    // check if it has pipe
    //printf("count pipe\n");
    int pipe_count=0;
    for(int i=0; command_list[i]!=NULL;i++){
        if (strcmp("|",command_list[i])==0){
            if(i==command_length){
                fprintf(stderr,"Error: invalid command\n");
                return 1;
            }
            pipe_count++;
        }
    }
    //printf("There are %d pipes.\n", pipe_count);
    //printf("program:\"%s\"",program);
    // check if it's a build-in command
    //printf("number of buildins: %d\n",buildin_num);
    //printf("check buildins\n");
    for (int i=0;i<buildin_num;i++){
        //printf("program is:%s\n",program);
        if (strcmp(program, buildin_list[i]) == 0){
            //printf("Buildin is %s",program);
            if (pipe_count>0){
                fprintf(stderr, "Error: invalid command\n"); 
                return 1;
            }
            //printf("Found MATCH buildin: %s\n",program);
            if(buildin_func[i](command_list)!=0){
                return 1;
            }
            //printf("buildin ran successfuly\n");
            return 0;
        }
    }

    //printf("It's not a build-in command.\n");
    int status=0;
    if (pipe_count==0){  //no pipe
        //printf("There's no pipe.\n"); 
        if(io_redirection(command_list, 0, 0)!=0){
            return 1;
        }
    }
    else{
        char*** pipe_list = parse_pipe(command_list, pipe_count);
        // printf("Parsed according to pipes\n");
        // printf("The pipelist is:\n");
        // for(int i=0;i<=pipe_count;i++){
        //     for(int j=0;pipe_list[i][j]!=NULL;j++){
        //         printf("%s ",pipe_list[i][j]);
        //     }
        //     printf("\n");
        // }
        //printf("About to run pipes\n");
        status = exec_pipe(pipe_list,pipe_count);
        //printf("Finished running pipes\n");

        for(int i=0;i<=pipe_count;i++){
            free(pipe_list[i]);
        }
        free(pipe_list);
        return status;
    }
    return 0;
}

