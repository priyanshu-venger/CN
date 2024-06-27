#ifndef sock
#define sock

#define T 5
#define probability_of_drop 0.05

#define MAX_BUFF_SIZE 1024
#define SOCK_MTP 3
#define MAX_SOCKETS 25

#include <netinet/in.h>
struct SOCK_INFO
{
    int sock_id;
    char IP[20];
    int port;
    int errno_val;
};
struct sender_window
{
    int wsize;
    int nack[5];
};
struct receiver_window
{
    int wsize;
    int nack[5];
};
struct sockets{
    int status;
    int pid;
    int sockid;
    int port;
    char ip[20];
    char sbuff[10][MAX_BUFF_SIZE+1],rbuff[5][MAX_BUFF_SIZE+1];
    struct sender_window swnd;
    struct receiver_window rwnd;
};

int m_socket(int family, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr_in *src_addr, socklen_t src_len,
           const struct sockaddr_in *dest_addr, socklen_t dest_len);
int m_sendto(int sockfd, const void *buf, size_t len, int flags,
             const struct sockaddr_in *dest_addr, socklen_t addrlen);
int m_recvfrom(int sockfd, void *buf, size_t len, int flags,
               struct sockaddr_in *src_addr, socklen_t *addrlen);
int m_close(int sockfd);

int dropMessage(float p);

#endif