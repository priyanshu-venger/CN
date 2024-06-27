#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include<pthread.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include "msocket.h"
#include<sys/select.h>
#include<time.h>
#include<signal.h>
#include<errno.h>

int max(int a,int b){
    return ((a>b)?a:b);
}

struct SOCK_INFO *si;
time_t stime[25][10];                                                                                       //stores last send time of message
int last_ack[25];                                                                                           //last index of sbuff which has been acknowledged
int eflag=0,last_ack_sno[25];                                                                               //eflag to handle ^C,last_ack_sno stores last acknowledged sequence no
int semwrite,semread,SemClose,count=0,count1=0;                                                             //holds semaphores ids
int M_bind(int sockfd,
           const struct sockaddr *source_addr, socklen_t source_len)
{
    printf("in bind\n");
    // Find the corresponding socket in the socket table
    if(bind(sockfd,source_addr,source_len)<0){
        si->errno_val=errno;
        si->sock_id=-1;
        return -1;
    }
    return 1;
}
int M_socket()
{
    printf("in socket\n");
    if((si->sock_id=socket(AF_INET,SOCK_DGRAM,0))<0){
        si->sock_id=-1;
        si->errno_val=errno;
        return -1;
    }
    return 1;
}

int dropMessage(float p) {
    float random_num = (float)rand() / RAND_MAX; // Generate random number between 0 and 1
    // printf("%f\n",random_num);
    if (random_num < p) {
        return 1; // Message dropped
    } else {
        return 0; // Message not dropped
    }
}

void *R(void* SM){
    fd_set fd;
    time_t atime[25];                                                                                       //stores time when ACK SIZE was sent
    int nfds=0,sno=0,flag=0,last[25];                                                                       //last stores index of rbuff where message was last stored on receive
    struct timeval tv;  
    struct sockets* sm=(struct sockets*)SM;                                                                 //holds shared memory SM
    struct sockaddr_in peer;                                                                                //stores other party's sockaddr info
    socklen_t size=sizeof(peer);
    char *buff,buff1[50]="",*buff2;
    char *loc;
    buff=(char*)malloc(MAX_BUFF_SIZE*5+100);
    int nospace[25],n,loc1=0,j;
    struct sembuf sem_wait = {0, -1, 0};
    struct sembuf sem_signal = {0, 1, 0};
    for(int i=0;i<25;i++){                                                                                  
        atime[i]=(time_t)(0);
        last[i]=-1;                                                                                         //it gets set to -1 if there are no messages in the rbuff
        nospace[i]=0;
    }
    while(!eflag){
        sem_wait.sem_num = 0;
        semop(SemClose,&sem_wait,1);                                                                        //waits if G tries to close a socket, so it doesn't cause any issue in select
        nfds=0;
        FD_ZERO(&fd);
        for(int i=0;i<25;i++){
            if((sm[i].status==1)||(sm[i].status==-2)){
                FD_SET(sm[i].sockid,&fd);
                nfds=max(nfds,sm[i].sockid);
            }   
        }
        tv.tv_sec=T;
        tv.tv_usec=0;
        // printf("in r\n");
        select(nfds+1,&fd,0,0,&tv);
        for(int i=0;i<25;i++){
            if(((sm[i].status==1)||(sm[i].status==-2))&&(FD_ISSET(sm[i].sockid,&fd))){                     //checks if there is any message to read
                memset(&peer,0,sizeof(peer));                                                              
                
                peer.sin_addr.s_addr=inet_addr(sm[i].ip);
                
                peer.sin_port=htons(sm[i].port);
                
                peer.sin_family=AF_INET;
                size=sizeof(peer);
                if((n=recvfrom(sm[i].sockid,buff,5*MAX_BUFF_SIZE+99,0,(struct sockaddr*)&peer,&size))<0){
                    perror("recv failed\n");
                }
                buff[n]='\0';


                // Call dropMessage to determine whether to drop the message or not
                if (dropMessage(probability_of_drop) == 1) {
                // Message dropped, do not process it
                continue; // Skip processing and wait for the next message
                }

                loc=0;
                buff2=strtok_r(buff,"\n",&loc);                                                           //breaks recieved message into fragments,1st fragment represents message type
                memset(&peer,0,sizeof(peer));
                peer.sin_addr.s_addr=inet_addr(sm[i].ip);
                peer.sin_port=htons(sm[i].port);
                peer.sin_family=AF_INET;
                flag=0;
                while(1){
                    
                    if(!buff2) break;
                    
                    if(!strcmp("DATA",buff2)){                                                          //if it is data type
                        sem_wait.sem_num = i;
                        semop(semread,&sem_wait,1);                                                     //lock semaphore for writing to buffer
                        buff2=strtok_r(0,"\n",&loc);                                                   //sequence number of message
                        sno=atoi(buff2);
                        buff2+=strlen(buff2)+1;                                                        //data part
                        if((last[i]==-1)||(sm[i].rbuff[(last[i])][0]=='\0')) last[i]=-1;                //if there are no rem. msgs in rbuff,reset last_ack to -1
                        if(sno==sm[i].rwnd.nack[0]){                                                    //checks if received message is next inorder message
                            if((sm[i].rbuff[(last[i]+1)%5][0]=='\0')){                                  //checks if index is empty,if it is not,then message has already been received,so skip
                                strncpy(sm[i].rbuff[(last[i]+1)%5],buff2,MAX_BUFF_SIZE);
                                sm[i].rbuff[(last[i]+1)%5][MAX_BUFF_SIZE]='\0';
                            }
                            sm[i].rwnd.wsize--;                                                         //decreases window size
                            last[i]=(last[i]+1)%5;                                                      //increments last_ack index
                            if(!sm[i].rwnd.wsize) nospace[i]=1;                                         //if there is no more space left
                            for(j=0;j<5;j++){
                                sm[i].rwnd.nack[j]=(sm[i].rwnd.nack[j])%16+1;                           //increments next expected seq. no.
                            }
                        }
                        else{
                            for(j=1;j<sm[i].rwnd.wsize;j++){                                            //if it is out of order message
                                if(sno==sm[i].rwnd.nack[j]){                                            //checks if it is in received window size,if not discard
                                    if((sm[i].rbuff[(last[i]+1+j)%5][0]=='\0')){                        //if already received discard
                                        strncpy(sm[i].rbuff[(last[i]+1+j)%5],buff2,MAX_BUFF_SIZE);  
                                        sm[i].rbuff[(last[i]+1+j)%5][MAX_BUFF_SIZE]='\0'; 
                                    }
                                    break;
                                }
                            }
                        }
                        flag=1;                                                                         //sets flag to illustrate that atleast one DATA message has been recieved to send ACK
                        buff2+=MAX_BUFF_SIZE+1;                                                         //skips data part of message
                        loc=0;
                        buff2=strtok_r(buff2,"\n",&loc);                                                //read next message
                        sem_signal.sem_num = i;
                        semop(semread,&sem_signal,1);
                    }
                    else {
                        if(!strcmp("ACK",buff2)){                                                       //checks if it is ACK type
                            sem_wait.sem_num = i;
                            semop(semwrite,&sem_wait,1);                                                //for mutual exclusion between S and R for so i and race condition between sendto
                            buff2=strtok_r(0,"\n",&loc);                                                //rwnd.wsize
                            int swnd=atoi(buff2);
                            buff2=strtok_r(0,"\n",&loc);                                                //sequence no.
                            sno=atoi(buff2);
                            if((sm[i].swnd.nack[0]<1)||((sno)%16+1==sm[i].swnd.nack[0])){               //if ack is for message which has been ack,then it is a DUPLICATE ACK(rwnd.size is non-zero)
                                // printf("sending ack size\n");                                        //,so send ACK SIZE
                                sm[i].swnd.wsize=swnd;                                                  
                                if(sendto(sm[i].sockid,"ACK SIZE\n",9,0,(struct sockaddr*)&peer,sizeof(peer))!=9){  
                                    printf("send failed\n");
                                }
                            }
                            else{
                                for(j=0;j<sm[i].swnd.wsize;j++){                                        //checks in swnd.nack for recvd. sequence no.
                                    if(sno==sm[i].swnd.nack[j]){                                        //sets loc1 to so index
                                        loc1=j;
                                        break;
                                    }
                                }
                                for(j=0;j<sm[i].swnd.wsize;j++){                                        //all messages till loc1 has been ack,so change send window
                                    if(j+loc1+1<sm[i].swnd.wsize){
                                        sm[i].swnd.nack[j]=sm[i].swnd.nack[j+loc1+1];
                                        stime[i][j]=stime[i][j+loc1+1];
                                    }
                                    else {                                                              //set to 0 for all indices which has not been yet sent
                                        sm[i].swnd.nack[j]=0;
                                        stime[i][j]=(time_t)(0);
                                    }
                                }
                                sm[i].swnd.wsize=swnd;
                                for(j=0;j<=loc1;j++){                                                 //empties all indices which have been ack
                                    strcpy(sm[i].sbuff[(j+last_ack[i]+1)%10],"\0");
                                }
                                last_ack[i]=(last_ack[i]+loc1+1)%10;                                    //updates last_ack according to last index ack
                                if(sm[i].sbuff[(last_ack[i]+1)%10][0]=='\0'){                           //if all messages in sbuff has been sent,reset last_ack to -1
                                    last_ack[i]=-1;
                                }
                                last_ack_sno[i]=sno;                                                    //sets seq no of last ack message

                            }
                            sem_signal.sem_num = i;
                            semop(semwrite,&sem_signal,1);
                        }
                        else if(!strcmp("ACK SIZE",buff2)){                                            
                            atime[i]=(time_t)(0);
                        }
                        buff2=strtok_r(NULL,"\n",&loc);
                    }
                }
                if(flag){
                    sem_wait.sem_num = i;
                    semop(semread,&sem_wait,1);
                    sprintf(buff1,"ACK\n%d\n%d\n",sm[i].rwnd.wsize,(sm[i].rwnd.nack[0]-1)?sm[i].rwnd.nack[0]-1:16);
                    sem_signal.sem_num = i;
                    semop(semread,&sem_signal,1);
                    sem_wait.sem_num = i;
                    semop(semwrite,&sem_wait,1);                                                                    //race condition between sendto
                    if(sendto(sm[i].sockid,buff1,strlen(buff1),0,(struct sockaddr*)&peer,size)!=(ssize_t)strlen(buff1)){
                        perror("send failed2\n");
                    }   
                    sem_signal.sem_num = i;
                    semop(semwrite,&sem_signal,1); 
                }
            }
        }
        if(!(tv.tv_sec+tv.tv_usec)){
            for(int i=0;i<25;i++){
                if((sm[i].status==1)||(sm[i].status==-2)){
                    memset(&peer,0,sizeof(peer));
                    peer.sin_addr.s_addr=inet_addr(sm[i].ip);
                    peer.sin_port=htons(sm[i].port);
                    peer.sin_family=AF_INET;
                    if(sm[i].rwnd.wsize&&(atime[i]!=(time_t)(0))){
                        if(difftime(time(NULL),atime[i])>=T){                                                           //if ACK for non-zero rwnd.wsize has not been ack within T s
                            
                            sprintf(buff1,"ACK\n%d\n%d\n",sm[i].rwnd.wsize,(sm[i].rwnd.nack[0]-1)?sm[i].rwnd.nack[0]-1:16); //resend it
                            sem_wait.sem_num = i;
                            semop(semwrite,&sem_wait,1);                                                                        //race condition between sendto
                            if(sendto(sm[i].sockid,buff1,strlen(buff1),0,(struct sockaddr*)&peer,size)!=(ssize_t)strlen(buff1)){
                                perror("send failed3\n");
                            }
                            sem_signal.sem_num = i;
                            semop(semwrite,&sem_signal,1); 
                            time(&atime[i]);
                            nospace[i]=0;
                        }
                    }
                    if(nospace[i]){                                                                                     //if rwnd.wsize >0,send duplicate ACK
                        if(sm[i].rwnd.wsize){
                            sprintf(buff1,"ACK\n%d\n%d\n",sm[i].rwnd.wsize,(sm[i].rwnd.nack[0]-1)?sm[i].rwnd.nack[0]-1:16);
                            sem_wait.sem_num = i;
                            semop(semwrite,&sem_wait,1);
                            if(sendto(sm[i].sockid,buff1,strlen(buff1),0,(struct sockaddr*)&peer,size)!=(ssize_t)strlen(buff1)){
                                perror("send failed4\n");
                            }
                            sem_signal.sem_num = i;
                            semop(semwrite,&sem_signal,1); 
                            time(&atime[i]);
                            nospace[i]=0;
                        }
                    }
                }
            }
        }
        sem_signal.sem_num =0;
        semop(SemClose,&sem_signal,1);
    }
    free(buff);
    pthread_exit(0);
}
void *S(void* SM){
    struct sockaddr_in peer;
    char buff[MAX_BUFF_SIZE*5+100];
    struct sockets* sm=(struct sockets*)SM;
    struct sembuf sem_wait = {0, -1, 0};
    struct sembuf sem_signal = {0, 1, 0};
    int flag=0,j,i;
    for(int i=0;i<25;i++){
        for(int j=0;j<10;j++)stime[i][j]=(time_t)(0);
        last_ack[i]=-1;
        last_ack_sno[i]=0;
    }
    while(!eflag){
        sleep((int)T/2);
        // printf("in s\n");
        for(i=0;i<25;i++){
            sem_wait.sem_num = i;
            semop(semwrite,&sem_wait,1);                                                            //locks for mutual exclusion between sendto and send related buffers update
            if((sm[i].status==1)||(sm[i].status==-2)){                                              //checks if the socket is active or not
                memset(&peer,0,sizeof(peer));
                peer.sin_addr.s_addr=inet_addr(sm[i].ip);
                peer.sin_port=htons(sm[i].port);
                peer.sin_family=AF_INET;
                strcpy(buff,"\0");
                flag=0;
                j=0;
                if((sm[i].swnd.nack[0]==-1)) {                                                      //nack[0] has been set to -1 on initialization,so checks for it
                    last_ack[i]=-1;                                                                 //if so resets last_ack,last_ack_sno
                    last_ack_sno[i]=0;
                }
                if((sm[i].swnd.nack[0]>0)&&(difftime(time(0),stime[i][0])>T)){                      //if ACK has not been received for T s since message last sent time
                    for(j=0;j<sm[i].swnd.wsize;j++){
                        if((sm[i].swnd.nack[j]>0)){                                                 //resends all messages for which ACK has not been recvd
                            sprintf(buff+strlen(buff),"DATA\n%d\n%s\n",sm[i].swnd.nack[j],sm[i].sbuff[(last_ack[i]+1+j)%10]);
                            time(&stime[i][j]);
                            count++;
                        }
                        else break;
                    }
                    if(sendto(sm[i].sockid,buff,strlen(buff),0,(struct sockaddr*)&peer,sizeof(peer))!=(ssize_t)strlen(buff)){
                        perror("send failed\n");
                        
                    }
                }
                strcpy(buff,"\0");
                flag=0;
                for(j=j;j<sm[i].swnd.wsize;j++){                                                //sends messages which has not been sent yet
                    if((sm[i].swnd.nack[j]<1)&&sm[i].sbuff[(last_ack[i]+1+j)%10][0]!='\0'){     //checks if it has not been yet sent and if the index is empty
                        sprintf(buff+strlen(buff),"DATA\n%d\n%s\n",(last_ack_sno[i]+j)%16+1,sm[i].sbuff[(last_ack[i]+1+j)%10]);
                        sm[i].swnd.nack[j]=(last_ack_sno[i]+j)%16+1;
                        time(&stime[i][j]);                                                     //sets time when it was sent
                        flag=1;                                                                 //sets flag if atleast one message is there which has not been sent
                        count1++;
                    }
                }
                if(flag){                                                                       //sends if so
                    if(sendto(sm[i].sockid,buff,strlen(buff),0,(struct sockaddr*)&peer,sizeof(peer))!= (ssize_t)strlen(buff)){
                        perror("send failed\n");

                    }  
                }
            }
            sem_signal.sem_num =i;
            semop(semwrite,&sem_signal,1);
        }
        
    }
    pthread_exit(0);
}
void *G(void* SM){
    struct sockets* sm=(struct sockets*)SM;
    struct sembuf sem_wait = {0, -1, 0};
    struct sembuf sem_signal = {0, 1, 0};
    while(!eflag){
        //sleep((int)T/2);
        sem_wait.sem_num = 0;
        semop(SemClose,&sem_wait,1);                                                            
        for(int i=0;i<25;i++){
            //closes sockets
            if(sm[i].status==-2){                                                               //if m_close has been closed on the socket,and all of the messages have been sent
                if(last_ack[i]==-1){
                    sem_wait.sem_num = i;                                                       //mutual exclusion with S,so that it doesn't close in between a sendto for i-th socket
                    semop(semwrite,&sem_wait,1);
                    printf("closing1\n");
                    close(sm[i].sockid);
                    sm[i].sockid=0;
                    sm[i].status=-1;
                    
                    sem_signal.sem_num =i;
                    semop(semwrite,&sem_signal,1);
                }
            }
            if(((sm[i].status>-1)&&(getpgid(sm[i].pid)<0))){
                sem_wait.sem_num = i;
                semop(semwrite,&sem_wait,1);                                                    //mutual exclusion with S,so that it doesn't close in between a sendto for i-th socket
                printf("closing\n");
                close(sm[i].sockid);
                sm[i].sockid=0;
                sm[i].status=-1;
                
                sem_signal.sem_num =i;
                semop(semwrite,&sem_signal,1);
               
            }
        }
        sem_signal.sem_num =0;
        semop(SemClose,&sem_signal,1);
    }
    pthread_exit(0);
}
int sem1;
pthread_t pd[3];
//for handling ^C signal,so that resources can be appropriately cleaned
void handler(int sig){
    if(sig==SIGINT){
        eflag=1;
        struct sembuf sem_signal = {0, 1, 0};
        semop(sem1,&sem_signal,1);
    }
}
int main(){
    struct sembuf sem_wait = {0, -1, 0};
    struct sembuf sem_signal = {0, 1, 0};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
    struct sockaddr_in sockadr;
    sem1=semget(ftok("/home",'a'),1,0777|IPC_CREAT);
    int sem2=semget(ftok("/home",'b'),1,0777|IPC_CREAT);
    semwrite = semget(ftok("/home",'E'),25,0777|IPC_CREAT);
    semread = semget(ftok("/home",'F'),25,0777|IPC_CREAT);
    SemClose = semget(ftok("/home",'G'),1,0777|IPC_CREAT);
    for(int i=0;i<25;i++) {
        semctl(semwrite,i,SETVAL,1);
        semctl(semread,i,SETVAL,1);
    }
    semctl(SemClose,0,SETVAL,1);
    semctl(sem1,0,SETVAL,0);
    semctl(sem2,0,SETVAL,0);
    int pid;
    int id=shmget(pid=ftok("/home",'A'),sizeof(struct sockets)*25,0777|IPC_CREAT);
    struct sockets *SM;
    int sockshm=shmget(ftok("/home",'B'),sizeof(struct SOCK_INFO),0777|IPC_CREAT);
    si= (struct SOCK_INFO*) shmat(sockshm,0,0);
    si->sock_id=0;
    SM=(struct sockets*)shmat(id,NULL,0);
    for(int i=0;i<25;i++) SM[i].status=-1;                                                  //sets status of all SM entry to -1
    pthread_create(&pd[0],&attr,R,(void*)SM);
    pthread_create(&pd[1],&attr,S,(void*)SM);
    pthread_create(&pd[2],&attr,G,(void*)SM);
    signal(SIGINT,handler);
    
    while(!eflag){
        semop(sem1,&sem_wait,1);
        if(eflag) break;
        if(!si->sock_id){
            M_socket();
        }
        else if(si->sock_id){
            memset(&sockadr,0,sizeof(sockadr));
            sockadr.sin_family=AF_INET;
            sockadr.sin_port=htons(si->port);
            sockadr.sin_addr.s_addr=inet_addr(si->IP);
            M_bind(si->sock_id,(struct sockaddr*)&sockadr,sizeof(sockadr));
        }
        semop(sem2,&sem_signal,1);
    }
    if(!pthread_join(pd[0],0)){
        printf("R joined\n");
    }
    if(!pthread_join(pd[1],NULL)){
        printf("S joined\n");
    }
    if(!pthread_join(pd[2],NULL)){
        printf("G joined\n");
    }
    //cleans resources
    shmdt(SM);
    shmdt(si);
    shmctl(id,IPC_RMID,NULL);
    pthread_attr_destroy(&attr);
    semctl(sem1,0,IPC_RMID,0);
    semctl(sem2,0,IPC_RMID,0);
    semctl(semwrite,0,IPC_RMID,0);
    semctl(SemClose,0,IPC_RMID,0);
    semctl(semread,0,IPC_RMID,0);
    shmctl(sockshm,IPC_RMID,0);
    //printf("%f",(count+count1)/(float)count1);
    return 0;
}