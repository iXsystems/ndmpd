


#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

void *threadMEM(void *par){
    char *a=(char*)malloc(1024*1024);
    printf("I'm doing nothing.\n");
    int i;
    for(i=0;i<1;i++)
        printf("doing ");
    free(a); 
    return (NULL);
}

void fork_t(){
     pthread_t thread;
     pthread_create(&thread, NULL, threadMEM, (void*)NULL);
     pthread_join(thread, NULL);
}

int main(void){
    printf("calling thread\n");

    while(1){
        fork_t();
        printf("thread ");
    }


    return 0;
}

