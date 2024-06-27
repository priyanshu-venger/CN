#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "msocket.h"

#include <sys/sem.h>
#include<sys/shm.h>

// Define semaphores
int sem1, sem2, SemClose, semread,semwrite;
// Define global variables
int last_write = -1, last_read = -1;

struct SOCK_INFO *sock_info;
struct sockets *SM;

// Function to attach shared memory
void attach_shared_memory() {

    // Attach sockets structure
    int shmid_SM = shmget(ftok("/home",'A'),sizeof(struct sockets)*25,0777|IPC_CREAT);
    if (shmid_SM == -1) {
        perror("shmget for sockets failed");
        exit(EXIT_FAILURE);
    }
    SM = (struct sockets*)shmat(shmid_SM,NULL,0);
    if (SM == (struct sockets *)(-1)) {
        perror("shmat for sockets failed");
        exit(EXIT_FAILURE);
    }

    // Attach SOCK_INFO structure
    int shmid_sock_info = shmget(ftok("/home",'B'),sizeof(struct SOCK_INFO),0777|IPC_CREAT);
    if (shmid_sock_info == -1) {
        perror("shmget for SOCK_INFO failed");
        exit(EXIT_FAILURE);
    }
    sock_info = (struct SOCK_INFO*) shmat(shmid_sock_info,0,0);
    if (sock_info == (struct SOCK_INFO *)(-1)) {
        perror("shmat for SOCK_INFO failed");
        exit(EXIT_FAILURE);
    }

}

// Function to detach shared memory
void detach_shared_memory() {
    // Detach SOCK_INFO structure
    if (shmdt((void *)sock_info) == -1) {
        perror("shmdt for SOCK_INFO failed");
        exit(EXIT_FAILURE);
    }

    // Detach sockets structure
    if (shmdt((void *)SM) == -1) {
        perror("shmdt for sockets failed");
        exit(EXIT_FAILURE);
    }
}



void reset_sock_info(struct SOCK_INFO* sock_info)
{
    sock_info->sock_id = 0;
    strcpy(sock_info->IP, "");
    sock_info->port = 0;
    sock_info->errno_val = -1;
}





int m_socket(int family, int type, int protocol)
{
    attach_shared_memory();
    SemClose = semget(ftok("/home", 'G'), 1, 0777 | IPC_CREAT);
    sem1 = semget(ftok("/home", 'a'), 1, 0777 | IPC_CREAT);
    sem2 = semget(ftok("/home", 'b'), 1, 0777 | IPC_CREAT);
    struct sembuf sem_wait = {0, -1, 0};
    struct sembuf sem_signal = {0, 1, 0};

    // Acquire SemClose for mutual exclusion
    semop(SemClose, &sem_wait, 1);

    // Acquire SemReadWrite for mutual exclusion
    if (type != SOCK_MTP)
    {
        // Release SemClose
        semop(SemClose, &sem_signal, 1);
        errno = EINVAL;
        detach_shared_memory();
        return -1;
    }
    // Find a free entry in the socket table as usual
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].status == -1)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        // Release SemClose
        semop(SemClose, &sem_signal, 1);
        errno = ENOBUFS; // No free entry available
        detach_shared_memory();
        return -1;
    }

    semop(sem1, &sem_signal, 1);
    // Wait for sem2
    semop(sem2, &sem_wait, 1);
    // Check SOCK_INFO
    if (sock_info->sock_id == -1)
    {
        errno = sock_info->errno_val;
        // Reset SOCK_INFO
        reset_sock_info(sock_info);

        detach_shared_memory();

        // Release SemClose
        semop(SemClose, &sem_signal, 1);
        return -1;
    }
    else
    {
        SM[i].sockid = sock_info->sock_id;
        SM[i].status = 0;
        SM[i].pid = getpid();
        // Reset SOCK_INFO
        reset_sock_info(sock_info);

        i=SM[i].sockid;
        detach_shared_memory();

        // Release SemClose
        semop(SemClose, &sem_signal, 1);
        return i;
    }
     // Release SemReadWrite

    // Release SemClose
    semop(SemClose, &sem_signal, 1);
    return -1;
}

int m_bind(int sockfd, const struct sockaddr_in *src_addr, socklen_t src_len,
           const struct sockaddr_in *dest_addr, socklen_t dest_len)
{
    attach_shared_memory();
    SemClose = semget(ftok("/home", 'G'), 1, 0777 | IPC_CREAT);
    sem1 = semget(ftok("/home", 'a'), 1, 0777 | IPC_CREAT);
    sem2 = semget(ftok("/home", 'b'), 1, 0777 | IPC_CREAT);
    struct sembuf sem_wait = {0, -1, 0};
    struct sembuf sem_signal = {0, 1, 0};

    // Acquire SemClose for mutual exclusion
    semop(SemClose, &sem_wait, 1);
    // Find the corresponding socket in the socket table
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].sockid == sockfd)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        errno = EINVAL; // Invalid socket descriptor
        detach_shared_memory();
        // Release SemClose
        semop(SemClose, &sem_signal, 1);
        return -1;
    }

    // Put the UDP socket ID, IP, and port in SOCK_INFO table
    sock_info->sock_id = SM[i].sockid;
    strcpy(sock_info->IP, inet_ntoa(src_addr->sin_addr));
    sock_info->port = ntohs(src_addr->sin_port);

    // Signal on sem1
    //struct sembuf sem_signal = {0, 1, 0};
    semop(sem1, &sem_signal, 1);

    // Wait for sem2
    //struct sembuf sem_wait = {0, -1, 0};
    semop(sem2, &sem_wait, 1);

    // Check SOCK_INFO
    if (sock_info->sock_id == -1)
    {
        errno = sock_info->errno_val;
        // Reset SOCK_INFO
        reset_sock_info(sock_info);
        detach_shared_memory();

        // Release SemClose
        semop(SemClose, &sem_signal, 1);
        return -1;
    }
    else
    {
        strcpy(SM[i].ip, inet_ntoa(dest_addr->sin_addr));
        SM[i].port = ntohs(dest_addr->sin_port);
        SM[i].status = 1;
        SM[i].rwnd.wsize = SM[i].swnd.wsize = 5;
        for (int j = 0; j < 5; j++)
        {
            SM[i].rwnd.nack[j] = j + 1;
            strcpy(SM[i].rbuff[j], "\0");
            SM[i].swnd.nack[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            strcpy(SM[i].sbuff[j], "\0");
        }
        // Reset SOCK_INFO
        SM[i].swnd.nack[0] = -1;
        SM[i].status=1;
        reset_sock_info(sock_info);
        detach_shared_memory();
         // Release SemReadWrite

        // Release SemClose
        semop(SemClose, &sem_signal, 1);
        return 0; // Success
    }
}

int m_sendto(int sockfd, const void *buf, size_t len, int flags,
             const struct sockaddr_in *dest_addr, socklen_t addrlen)
{
    attach_shared_memory();

    // Acquire SemClose for mutual exclusion
    // Acquire SemReadWrite for mutual exclusion

    int i;
    char *message = (char *)buf;
    semwrite = semget(ftok("/home",'E'),25,0777|IPC_CREAT);
    // Find the corresponding socket in the socket table
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        struct sembuf sem_wait = {i, -1, 0};
        struct sembuf sem_signal = {i, 1, 0};

        
        semop(semwrite, &sem_wait, 1);
        if (SM[i].sockid == sockfd)
        {
            // Check if the socket is bound
            if (SM[i].status == 0 || SM[i].port == 0 || SM[i].ip[0] == '\0')
            {
                // Release SemReadWrites
                semop(semwrite, &sem_signal, 1);
                errno = ENOTCONN;
                detach_shared_memory();
                return -1;
            }

            // Check if there is space in the send buffer
            int index = -1;
            if ((last_write == -1) || (!strlen(SM[i].sbuff[last_write])))
                last_write = -1;
            if (!strcmp(SM[i].sbuff[((last_write + 1) % 10)], "\0"))
            {
                strncpy(SM[i].sbuff[((last_write + 1) % 10)], message, len);
                last_write = (last_write + 1) % 10;
                SM[i].sbuff[index][len] = '\0'; // Null-terminate the string
                index = last_write;
            }
            if (index == -1)
            {
                
                semop(semwrite, &sem_signal, 1);

                errno = ENOBUFS;
                detach_shared_memory();
                return -1; // No space available in the send buffer
            }

            // Check if destination IP/Port matches bound IP/Port
            if (dest_addr == NULL || dest_addr->sin_port != htons(SM[i].port) ||
                strcmp(inet_ntoa(dest_addr->sin_addr), SM[i].ip) != 0)
            {
                
                semop(semwrite, &sem_signal, 1);
                errno = ENOTCONN;
                detach_shared_memory();
                return -1;
            }
    
            detach_shared_memory();
            semop(semwrite, &sem_signal, 1);
            return len;
        }
        semop(semwrite, &sem_signal, 1);
    }

    // Release SemReadWrite

    errno = EINVAL; // Invalid socket descriptor
    detach_shared_memory();
    return -1;
}

int m_recvfrom(int sockfd, void *buf, size_t len, int flags,
               struct sockaddr_in *src_addr, socklen_t *addrlen)
{
    attach_shared_memory();

    
    int i;
    char *message = (char *)buf;
    semread = semget(ftok("/home",'F'),25,0777|IPC_CREAT);
    // Find the corresponding socket in the socket table
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        struct sembuf sem_wait = {i, -1, 0};
        struct sembuf sem_signal = {i, 1, 0};
        semop(semread,&sem_wait,1);
        if (SM[i].sockid == sockfd)
        {
            // Check if the buffer is provided
            if (buf == NULL)
            {
                // Release SemReadWrite

                // Release SemClose
                semop(semread, &sem_signal, 1);

                errno = EINVAL;
                detach_shared_memory();
                return -1;
            }

            // Check if there are any messages in the receive buffer
            if ((strlen(SM[i].rbuff[(last_read + 1) % 5])) == 0)
            {
                // Release SemReadWrite
                semop(semread, &sem_signal, 1);

                last_read = -1;
                errno = ENOMSG;
                detach_shared_memory();
                return -1;
            }

            // Copy the first message from the receive buffer
            strncpy(message, SM[i].rbuff[(last_read + 1) % 5], len);
            message[len]=SM[i].rbuff[(last_read + 1) % 5][len]='\0';
            //printf("----%s-------\n",SM[i].rbuff[(last_read + 1) % 5]);
            last_read = (last_read + 1) % 5;
            strcpy(SM[i].rbuff[(last_read) % 5],"\0");
            if ((strlen(SM[i].rbuff[(last_read+1) % 5])) == 0)
                last_read = -1;
            SM[i].rwnd.wsize++;

            // Release SemReadWrite
            semop(semread, &sem_signal, 1);

            detach_shared_memory();

            return strlen(message);
        }
        semop(semread, &sem_signal, 1);
    }

    errno = EINVAL; // Invalid socket descriptor

    detach_shared_memory();
    return -1;
}

int m_close(int sockfd)
{
    attach_shared_memory();

    SemClose = semget(ftok("/home", 'G'), 1, 0777 | IPC_CREAT);

    struct sembuf sem_wait = {0, -1, 0};
    struct sembuf sem_signal = {0, 1, 0};

    // Acquire SemClose for mutual exclusion
    int i;
    semread = semget(ftok("/home",'F'),25,0777|IPC_CREAT);
    semwrite = semget(ftok("/home",'E'),25,0777|IPC_CREAT);
    // Find the corresponding socket in the socket table
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].sockid == sockfd)
        {
            sem_wait.sem_num=0;
            semop(SemClose, &sem_wait, 1);
            sem_wait.sem_num=i;
            semop(semwrite,&sem_wait,1);
            semop(semread,&sem_wait,1);
            // Close the socket
            //close(sockfd);
            SM[i].status=-2;
            // Mark the socket entry as free
            // memset(&SM[i], 0, sizeof(struct sockets));
            sem_signal.sem_num=0;
            // Release SemClose
            semop(SemClose, &sem_signal, 1);
            sem_signal.sem_num=i;
            semop(semwrite,&sem_signal,1);
            semop(semread,&sem_signal,1);
            detach_shared_memory();
            last_read = last_write = -1;
            return 0;
        }
    }

    errno = EINVAL; // Invalid socket descriptor
    detach_shared_memory();
    return -1;
}