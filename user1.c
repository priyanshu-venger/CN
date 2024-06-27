#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "msocket.h"

#define BUFFER_SIZE 1024
#define MAX_FILE_NAME_LENGTH 1000

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <local_IP> <local_port> <remote_IP> <remote_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Prompt the user to input the name of the file to be transferred
    char send_file_name[MAX_FILE_NAME_LENGTH];
    printf("Enter the name of the file to be sent: ");
    scanf("%[^\n]", send_file_name);
    getchar();
    send_file_name[strlen(send_file_name)] = '\0';
    // Open the file to be transferred
    FILE *file = fopen(send_file_name, "rb");
    if (file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char *local_ip = argv[1];
    int local_port = atoi(argv[2]);
    char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);

    // Create socket M1
    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sockfd == -1)
    {
        perror("m_socket");
        exit(EXIT_FAILURE);
    }

    // Bind M1 to local IP and Port
    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    src_addr.sin_addr.s_addr = inet_addr(local_ip);
    src_addr.sin_port = htons(local_port);

    // Set destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(remote_ip);
    dest_addr.sin_port = htons(remote_port);

    // Bind M1
    int bind_result = m_bind(sockfd, &src_addr, sizeof(src_addr), &dest_addr, sizeof(dest_addr));
    if (bind_result == -1)
    {
        perror("m_bind");
        exit(EXIT_FAILURE);
    }

    // Read data from the file and send it using m_sendto()
    char buffer[BUFFER_SIZE+1];
    size_t bytes_read;
    int count=0;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) == BUFFER_SIZE)
    {
        buffer[bytes_read]='\0';
        // Send the entire buffer
        int send_result ;
        while((send_result= m_sendto(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr_in *)&dest_addr, sizeof(dest_addr))==-1)){

            if (send_result == -1)
            {
                //perror("m_sendto");
                //exit(EXIT_FAILURE);
            }
        }
        //if(++count==7) break;
        printf("%s\n",buffer);
    }

    // Check for fread errors
    if (ferror(file))
    {
        perror("fread");
        exit(EXIT_FAILURE);
    }

    // If the last read was not a full buffer, pad the remaining bytes with a delimiter
    if (bytes_read > 0)
    {
        // Create a buffer to store the remaining bytes and the delimiter
        char remaining_buffer[BUFFER_SIZE+1];

        // Copy the remaining bytes to the buffer
        memcpy(remaining_buffer, buffer, bytes_read);

        // Fill the rest of the buffer with the delimiter
        memset(remaining_buffer + bytes_read, '\r', BUFFER_SIZE - bytes_read); // Adjust the delimiter as needed
        remaining_buffer[BUFFER_SIZE]='\0';
        // Send the remaining bytes and the padded delimiter together
        int send_result;// = m_sendto(sockfd, remaining_buffer, BUFFER_SIZE, 0, (struct sockaddr_in *)&dest_addr, sizeof(dest_addr));
        while((send_result= m_sendto(sockfd, remaining_buffer, BUFFER_SIZE, 0, (struct sockaddr_in *)&dest_addr, sizeof(dest_addr))==-1)){

            if (send_result == -1)
            {
                //perror("m_sendto");
                //exit(EXIT_FAILURE);
            }
        }
        printf("%s\n",remaining_buffer);
    }

    // Close the file
    fclose(file);
    sleep(5);
    // Close socket M1
    if (m_close(sockfd) == -1)
    {
        perror("m_close");
        exit(EXIT_FAILURE);
    }

    return 0;
}