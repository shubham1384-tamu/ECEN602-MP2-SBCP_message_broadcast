#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>

#define PORT 12345
#define MAX_CLIENTS 10

// SBCP Header
typedef struct {
    uint16_t type;
    uint16_t length;
} SBCP_Header;

// SBCP Attribute
typedef struct {
    uint16_t type;
    uint16_t length;
    char payload[512];  // Adjusted to maximum expected payload size
} SBCP_Attribute;

// SBCP Message
typedef struct {
    SBCP_Header header;
    SBCP_Attribute attributes[2];  // For simplicity, assuming maximum of two attributes per message
} SBCP_Message;

// Message types
#define MSG_JOIN 2
#define MSG_FWD 3
#define MSG_SEND 4

int main() {
    int server_fd, new_socket, client_socket[MAX_CLIENTS], max_clients = MAX_CLIENTS, activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;
    char buffer[1024];  // Data buffer
    fd_set readfds;

    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    puts("Waiting for connections ...");

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
             
        for ( i = 0 ; i < max_clients ; i++) {
            sd = client_socket[i];
            if(sd > 0)
                FD_SET(sd, &readfds);
            if(sd > max_sd)
                max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            for (i = 0; i < max_clients; i++) {
                if(client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }
          
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
              
            if (FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, sizeof(buffer));
                if (valread == 0) {
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd);
                    client_socket[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    printf("Message received: %s\n", buffer);
                    // Further SBCP message parsing and handling here
                }
            }
        }
    }
      
    return 0;
}
