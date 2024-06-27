#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "msocket.h"

#define BUFFER_SIZE 1024
#define MAX_FILE_NAME_LENGTH 1000

// Function to find a delimiter in a buffer
int find_delimiter(const char *buffer, size_t size, char delimiter) {
    for (size_t i = 0; i < size; i++) {
        if (buffer[i] == delimiter) {
            return 1; // Delimiter found
        }
    }
    return 0; // Delimiter not found
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <local_IP> <local_port> <remote_IP> <remote_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Prompt the user to input the name of the received file
    char receive_file_name[MAX_FILE_NAME_LENGTH];
    printf("Enter the name of the file that you want to receive as: ");
    scanf("%[^\n]", receive_file_name);
    getchar();
    receive_file_name[strlen(receive_file_name)] = '\0';

    // Create a new file to write the received data
    FILE *file = fopen(receive_file_name, "wb");
    if (file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char *local_ip = argv[1];
    int local_port = atoi(argv[2]);
    char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);

    // Create socket M2
    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sockfd == -1)
    {
        perror("m_socket");
        exit(EXIT_FAILURE);
    }

    // Bind M2 to local IP and Port
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

    // Bind M2
    int bind_result = m_bind(sockfd, &src_addr, sizeof(src_addr), &dest_addr, sizeof(dest_addr));
    if (bind_result == -1)
    {
        perror("m_bind");
        exit(EXIT_FAILURE);
    }


    // Buffer to store received data
    char buffer[BUFFER_SIZE+1];

    // Receive data using m_recvfrom() and write it to the file
    int bytes_received;
    do
    {
        // Receive data
        bytes_received = m_recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
        
        if (bytes_received == -1)
        {
            // Error occurred
            //perror("m_recvfrom");
            //exit(EXIT_FAILURE);
        }
        else if (bytes_received > 0)
        {
            int flag=0;

            // Remove '\r' from the received data
            int index = 0; // Index for the modified buffer
            for (int i = 0; i < bytes_received; i++)
            {
                if (buffer[i] != '\r')
                {
                    buffer[index++] = buffer[i];
                }
                else flag = 1;  // Check if '\r' is encountered in the received data
            }
            buffer[index] = '\0'; // Null-terminate the modified buffer
            
            printf("%s\n",buffer);

            // Data received, write it to the file
            if (fwrite(buffer, 1, index, file) != (size_t)index)
            {
                perror("fwrite");
                exit(EXIT_FAILURE);
            }

            if (flag==1)
            {
                break; // Stop receiving if delimiter is found
            }

            bytes_received = 0; // keep on receiving
        }
    } while (bytes_received <= 0);

    // Close the file
    fclose(file);
    
    // Close socket M2
    if (m_close(sockfd) == -1)
    {
        perror("m_close");
        exit(EXIT_FAILURE);
    }

    return 0;
}