#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include<errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048


#define JOIN    2
#define SEND    4
#define FWD     3


struct SBCP_header
{
    unsigned int version:9;
    unsigned int type:7;  // JOIN, SEND, FWD
    unsigned int length:2;
    unsigned int payload:1;
};

struct SBCP_payload
{
    char username[16];
    char message[512];
    char reason[32];
    char client_count[2];
};
struct SBCP_attribute
{
    unsigned int type;
    unsigned int length;
    struct SBCP_payload payload;
};

struct SBCP_packet
{
    struct SBCP_header header;
    struct SBCP_attribute attribute;

};

struct SBCP_packet decode_data(char* buffer_input)
{
    struct SBCP_packet rec;
    int h_type;
    int h_len;
    int h_version;

    sscanf(buffer_input,"%d:%d:%d:%99[^:]:%99s",&h_type,&h_len,&h_version,rec.attribute.payload.username,rec.attribute.payload.message);
    rec.header.type=h_type;
    rec.header.length=h_len;
    rec.header.version=h_version;
    printf("Packet decoded: %d:%d:%d:%s:%s\n",rec.header.type,rec.header.length,rec.header.version,rec.attribute.payload.username,rec.attribute.payload.message);
 //int valread=read(client_sock, buffer_input, sizeof(buffer_input));
 if(rec.header.type==SEND)
 printf("Username  = %s, Message = %s\n",rec.attribute.payload.username,rec.attribute.payload.message);
 //printf("type= %d\n",rec.header.type);
 //sscanf(buffer_input,"%d:%d:%d:%s",&header,&type,&version,username);

 return rec;
 //return valread;
}

int client_sockets[MAX_CLIENTS];
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
char client_hostname[MAX_CLIENTS][20];

void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    int read_size;
    char client_message[BUFFER_SIZE];
    char msg_payload[512];
    //struct SBCP_packet test;

    while ((read_size = recv(sock, client_message, BUFFER_SIZE, 0)) > 0) {
    struct SBCP_packet test=decode_data(client_message);

     if(test.header.type == JOIN)
     {
    //printf("username %s is trying to join\n",test.attribute.payload.username);
    
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if(strcmp(test.attribute.payload.username,client_hostname[i])==0)
        {
            printf("User is already present. Disconnecting...\n");
        }
    }
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(client_sockets[i]==sock)
        {
            for(int j=0;j<strlen(test.attribute.payload.username);j++)
            client_hostname[i][j]=test.attribute.payload.username[j];
            printf("Client name added: %s\n",client_hostname[i]);
            break;
        }
    }
        continue;
     }
     
    //while ((read_size = recv(sock, &test, sizeof(test), 0)) > 0){
        /*
        if(test.header.type==JOIN)
        {
        printf("username joined: %s\n",test.attribute.payload.username);
        }
        if(test.header.type==SEND)
        {
        strcpy(msg_payload,test.attribute.payload.message);
        printf("Message received: %s\n",test.attribute.payload.message);
        
        }
        */

        // Send the message to all clients
        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] != 0 && client_sockets[i] != sock) {
                send(client_sockets[i], client_message, strlen(client_message), 0);
            }
        }
        pthread_mutex_unlock(&client_mutex);

        memset(client_message, 0, BUFFER_SIZE);  // Clear the buffer
    }

    if (read_size == 0) {
        puts("Client disconnected");
        close(sock);
    } else if (read_size == -1) {
        perror("recv failed");
    }

    // Remove the socket from the array
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == sock) {
            client_sockets[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    free(socket_desc);
    return 0;
}

int main(int argc, char *argv[]) {
    int socket_desc, new_socket, c, *new_sock;
    struct sockaddr_in server, client;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
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
