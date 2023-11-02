#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "nyush.h"

int main(){

    while(1){
        ignore_signals();
        size_t size = 1000;
        char* command = malloc(size*sizeof(char));
        char** command_list;
        prompt();
        if(getline(&command, &size, stdin)==-1){
            exit(0);
        }
        command_list = parse_line(command);
        executor(command_list);
        free(command);
        free(command_list);
        sleep(0.5);
    }
}