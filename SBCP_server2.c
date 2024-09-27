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
#include <netdb.h>



#define MAX_CLIENTS 3
#define BUFFER_SIZE 2048

#define JOIN    2
#define SEND    4
#define FWD     3
#define NAK     5
#define ACK     7

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
int clients_connected=0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_nak_message(int sock, const char* reason) {
    char nak_message[BUFFER_SIZE];
    int msg_len = snprintf(nak_message, sizeof(nak_message), "5:0:3::%s", reason);
    send(sock, nak_message, msg_len, 0);
}
void send_stat_message(int sock, const char* status) {
    char stat_message[BUFFER_SIZE];
    printf("Stat message: %s\n",stat_message);
    int msg_len = snprintf(stat_message, sizeof(stat_message), "4:0:3::%s", status);
    send(sock, stat_message, msg_len, 0);
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
                        printf("Username added: %s\n", packet.attribute.payload.username);
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
        clients_connected-=1;
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

    //Handling IPv4 and IPV6
    struct addrinfo GEN_IPv4_6, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    if (argc != 2) {
    fprintf(stderr,"usage: showip hostname\n");
    return 1;
    }

    memset(&GEN_IPv4_6, 0, sizeof GEN_IPv4_6);
    GEN_IPv4_6.ai_family = AF_UNSPEC; //For IPv4 adn IPv6 AF_INET or AF_INET6 to force version
    GEN_IPv4_6.ai_socktype = SOCK_STREAM;

    //check if there is error in IPV4 or IPv6
    if ((status = getaddrinfo(argv[1], NULL, &GEN_IPv4_6, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
}
    for(p = res;p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;
        int socket_desc, new_socket, c, *new_sock;
        struct addrinfo *pp;

        // we should handle Client too
        struct sockaddr_in client;

        pp=p->ai_family;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (pp == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            server = &(ipv4->sin_addr);
            ipver = "IPv4";
            server.sin_family = AF_INET;
            server.sin_addr.s_addr = argv[1];
            server.sin_port = htons(8888);
            //create a socket that is compatible with IPv4
            socket_desc = socket(pp, GEN_IPv4_6.ai_socktype, 0);
            if (socket_desc == -1) {
                printf("Could not create socket");
                return 1;
                            }
            if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
                perror("bind failed. Error");
                return 1;
                            }

        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            server = &(ipv6->sin6_addr);
            ipver = "IPv6";
            server.sin_family = AF_INET6;
            server.sin_addr.s_addr = argv[1];
            server.sin_port = htons(8888);
            //create a socket that is compatible with IPv4 and IPV6
            socket_desc = socket(pp, GEN_IPv4_6.ai_socktype, 0);
            if (socket_desc == -1) {
                printf("Could not create socket");
                return 1;
                            }
            if (bind(socket_desc, (struct sockaddr_in6 *)&server, sizeof(server)) < 0) {
                perror("bind failed. Error");
                return 1;
                            }
        }

        // convert the IP to a string and print it:
        //inet_ntop(pp, server, ipstr, sizeof ipstr);
        //printf("  %s: %s\n", ipver, ipstr);
}


    listen(socket_desc, MAX_CLIENTS);
    puts("Waiting for incoming connections...");

    if (ipver == "IPv4") {
                c = sizeof(struct sockaddr_in);
                            }
    else {
        c = sizeof(struct sockaddr_in6)

    }
    



    while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;

        //printf("Num of clients connected: %d\n",clients_connected);
        if(clients_connected==MAX_CLIENTS)
        {
            printf("Max limit reached");
            clients_connected+=1;  //later this value is decremented
            send_nak_message(new_socket, "Maximum limit reached\n");
        }
        else
        {
        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                clients_connected+=1;
                break;
            }
        }
        printf("Num of clients connected: %d\n",clients_connected);
        pthread_mutex_unlock(&client_mutex);
        }

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
