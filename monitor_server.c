#include <stdio.h>  // printf
#include <stdlib.h> // exit
#include <unistd.h> // close
#include <arpa/inet.h>

#define TRUE 1
#define FALSE 0


/**
 * @brief Reads a message from the socket and writes it to the given buffer.
 *
 * Marked inline to avoid the overhead of a function call.
 *
 * @param sd Accepted socket connection.
 * @param retBuf Buffer where read data is stored.
 * @param size Maximum buffer size, must be at least 1.
 * @return 0 if successful, -1 otherwise
 */
static inline int receive(const int sd, char* retBuf, const int size)
{
    int totSize = 0;
    while (totSize < size)
    {
        const int currSize = recv(sd, &retBuf[totSize], size - totSize, 0);
        if (currSize <= 0)
            // An error occurred
            return -1;
        totSize += currSize;
    }
    return 0;
}

int main(int argc, char* args[])
{
    if (argc < 2)
    {
        printf("Usage: %s <port>\n", args[0]);
        exit(EXIT_FAILURE);
    }
    // Parse port argument
    const int port = strtol(args[1], NULL, 10);
    struct sockaddr_in servaddr = {
        sin_family: AF_INET,
        sin_port : htons(port),
        sin_addr : {
            s_addr: INADDR_ANY
        },
    };
    // Create a new socket
    const int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Bind the socket to the specified port number
    if (bind(socketfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Socket bind failed");
        exit(EXIT_FAILURE);
    }
    // Set the maximum queue length for clients requesting connection to 5
    if (listen(socketfd, 5) < 0)
    {
        perror("Socket connection listen failed");
        exit(EXIT_FAILURE);
    }
    // Accept and serve all incoming connections in a loop
    while (TRUE)
    {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        printf("[Monitor server]: Ready, waiting for incoming connections.\n");
        const int new_socket = accept(socketfd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0)
        {
            perror("Socket accept failed");
            exit(EXIT_FAILURE);
        }
        printf("[Monitor server]: Accepted connection from %s\n", inet_ntoa(address.sin_addr));
        int nConsumers = 0;
        if (receive(new_socket, (char*)&nConsumers, sizeof(int)) < 0)
        {
            perror("Socket receive failed");
            exit(EXIT_FAILURE);
        }
        printf("[Monitor server]: Correctly received the number of consumers: %d.\n", nConsumers);
        int monitor_msg[nConsumers + 2];
        while (TRUE)
        {
            if (receive(new_socket, (char*)&monitor_msg, sizeof(monitor_msg)) < 0)
            {
                //perror("Socket receive failed");
                //exit(EXIT_FAILURE);
                printf("[Monitor server]: Stopped receiving messages from %s\n", inet_ntoa(address.sin_addr));
                break;
            }
            printf("[Monitor server]: queue: %d, produced: %d", ntohl(monitor_msg[0]), ntohl(monitor_msg[1]));
            for (int i = 2; i < nConsumers + 2; i++)
            {
                printf(", [%d]: %d", i - 2, ntohl(monitor_msg[i]));
            }
            printf("\n");
        }
        printf("[Monitor server]: Closing connection with %s\n", inet_ntoa(address.sin_addr));
        close(new_socket);
    }
    return 0; // Unreachable
}