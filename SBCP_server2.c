#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define MAX_CLIENTS 5
#define BUFFER_SIZE 2048

#define JOIN    2
#define SEND    4
#define FWD     3
#define NAK     5

struct SBCP_header {
    unsigned int version:9;
    unsigned int type:7;
    unsigned int length:16;
};

struct SBCP_payload {
    char username[16];
    char message[512];
    char reason[32];
    char client_count[2];
};

struct SBCP_attribute {
    unsigned int type;
    unsigned int length;
    struct SBCP_payload payload;
};

struct SBCP_packet {
    struct SBCP_header header;
    struct SBCP_attribute attribute;
};

struct SBCP_packet decode_data(char* buffer_input) {
    struct SBCP_packet rec;
    int h_type;
    int h_len;
    int h_version;
    sscanf(buffer_input, "%d:%d:%d:%99[^:]:%99s",&h_type,&h_len,&h_version,rec.attribute.payload.username,rec.attribute.payload.message);
    rec.header.type=h_type;
    rec.header.length=h_len;
    rec.header.version=h_version;
    printf("Packet decoded: Type=%d, Length=%d, Version=%d, Username=%s, Message=%s\n",
           rec.header.type, rec.header.length, rec.header.version,
           rec.attribute.payload.username, rec.attribute.payload.message);
    return rec;
}

int client_sockets[MAX_CLIENTS];
char client_hostnames[MAX_CLIENTS][20] = {0};
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_nak_message(int sock, const char* reason) {
    char nak_message[BUFFER_SIZE];
    int msg_len = snprintf(nak_message, sizeof(nak_message), "5:0:3::%s", reason);
    send(sock, nak_message, msg_len, 0);
}

void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    int read_size;
    char client_message[BUFFER_SIZE];

    while ((read_size = recv(sock, client_message, BUFFER_SIZE, 0)) > 0) {
        struct SBCP_packet packet = decode_data(client_message);

        if (packet.header.type == JOIN) {
            int username_found = 0;
            pthread_mutex_lock(&client_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if ((strcmp(packet.attribute.payload.username, client_hostnames[i]) == 0) && client_sockets[i]!=sock) {
                    username_found = 1;
                    break;
                }
            }

            if (username_found) {
                printf("Username %s is already taken. Disconnecting...\n", packet.attribute.payload.username);
                send_nak_message(sock, "Username is already taken");
                pthread_mutex_unlock(&client_mutex);
                break;  // Exit the loop and end thread
            } else {
                
                
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] == sock) {
                        strcpy(client_hostnames[i], packet.attribute.payload.username);
                        printf("Username added: %s\n", client_hostnames[i]);
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&client_mutex);
        } else {
            // Broadcast message to other clients
            pthread_mutex_lock(&client_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] != 0 && client_sockets[i] != sock) {
                    send(client_sockets[i], client_message, read_size, 0);
                }
            }
            pthread_mutex_unlock(&client_mutex);
        }
    }

    if (read_size == 0) {
        puts("Client disconnected");
    } else if (read_size == -1) {
        perror("recv failed");
    }

    close(sock);

    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == sock) {
            client_sockets[i] = 0;
            memset(client_hostnames[i], 0, sizeof(client_hostnames[i]));  // Clear the username
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    free(socket_desc);
    return NULL;
}

int main(int argc, char *argv[]) {
    int socket_desc, new_socket, c, *new_sock;
    struct sockaddr_in server, client;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        return 1;
    }

    listen(socket_desc, 3);
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

    while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;

        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);

        if (pthread_create(&sniffer_thread, NULL, client_handler, (void*) new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }
    }

    if (new_socket < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}
