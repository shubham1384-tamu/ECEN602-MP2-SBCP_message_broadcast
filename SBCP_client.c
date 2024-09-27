#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <netdb.h>


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888
#define BUFFER_SIZE 2048
#define JOIN    2
#define SEND    4
#define FWD     3
#define NAK     5

char username[16];
char buff[10000];

/**/
char msg_packet[10000];
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

void msg_to_char(struct SBCP_packet p)
{
    //char buff[2048];
    memset(msg_packet, 0, 2048);
    sprintf(msg_packet, "%d:%d:%d:%s:%s",p.header.type,p.header.length,p.header.version,p.attribute.payload.username,p.attribute.payload.message); 
    printf("buff: %s\n",msg_packet);
    //return buff;
}

void client_join(int client_sock)
{
    struct SBCP_packet p;
    p.header.type=JOIN;
    p.header.version=3;
    p.header.length=0;
    char u_name[50];
    printf("Enter username: ");
    scanf("%s",u_name);
    //char* Username="shubham";
    for(int i=0;i<strlen(u_name);i++)
    {
    p.attribute.payload.username[i]=u_name[i];
    username[i]=u_name[i];
    }
    //strcpy(p.attribute.payload.message,"");
    p.attribute.payload.message[0]='\0';
   int packet_size = sizeof(p.header) + sizeof(p.attribute);
   char* buffer[512];
    //sprintf(buff, "%d:%d:%d:%s:%s",p.header.type,p.header.length,p.header.version,p.attribute.payload.username,p.attribute.payload.message); 
    //send(client_sock, buff, strlen(buff), 0);
    msg_to_char(p);
    send(client_sock, msg_packet,sizeof(msg_packet),0);

    printf("Connecting....\n");
}

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
 if(rec.header.type == JOIN)
 printf("username %s joined\n",rec.attribute.payload.username);
 else if (rec.header.type ==NAK)
 {
    printf("NAK received\n");
    printf("reason: %s\n",rec.attribute.payload.message);
    //printf("Username already present");
    exit(0);
 }
 else
 printf("Message from %s: %s\n",rec.attribute.payload.username,rec.attribute.payload.message);
 //printf("type= %d\n",rec.header.type);
 //sscanf(buffer_input,"%d:%d:%d:%s",&header,&type,&version,username);
 
 return rec;
 //return valread;
}

struct SBCP_packet send_message(char* buffer)
{
    struct SBCP_packet p;
    p.header.type=SEND;
    p.header.version=3;
    p.header.length=0;
    //char u_name[50]="test";
    //printf("Enter message: ");
    //char* Username="shubham";
    for(int i=0;i<strlen(username);i++)
    {
    p.attribute.payload.username[i]=username[i];
    }
    for(int i=0;i<strlen(buffer);i++)
    {
        p.attribute.payload.message[i]=buffer[i];
    }
    //printf("struct created: %d:%d:%d:%s:%s\n",p.header.type,p.header.length,p.header.version,p.attribute.payload.username,p.attribute.payload.message);
    return p;
    //send(client_sock, &p,packet_size,0);
    //client_send(client_sock,&p1);
}


void *receive_message(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char server_reply[BUFFER_SIZE];
    int read_size;
    
    while((read_size = recv(sock, server_reply, sizeof(server_reply), 0)) > 0) {
        server_reply[read_size] = '\0';
        /*Experiment*/
        struct SBCP_packet rd=decode_data(server_reply);
        /********** */
        printf("Message from %s: %s",rd.attribute.payload.username,rd.attribute.payload.message);
        //printf("\nServer broadcasted: %s\n", server_reply);
        memset(server_reply, 0, sizeof(server_reply));
    }
    
    if (read_size == 0) {
        puts("Server disconnected");
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv failed");
    }
    
    free(socket_desc);
    return 0;
}

int main(int argc, char *argv[]) {
    int sock;
    int socket_desc;
    char message[BUFFER_SIZE], server_reply[BUFFER_SIZE];
    
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
        
        struct addrinfo *pp;

        pp=p->ai_family;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (pp == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            struct sockaddr_in server*;
            server= &(ipv4->sin_addr);
            ipver= "IPv4";
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
            // Connect to remote server
            if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
                perror("connect failed. Error");
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
                        // Connect to remote server
            if (connect(socket_desc, (struct sockaddr_in6 *)&server, sizeof(server)) < 0) {
                perror("connect failed. Error");
                return 1;
                        }
        }

        // convert the IP to a string and print it:
        //inet_ntop(pp, server, ipstr, sizeof ipstr);
        //printf("  %s: %s\n", ipver, ipstr);
    }

    
    puts("Connected\n");

    // Create a thread for receiving messages from the server
    pthread_t recv_thread;
    int *new_sock = malloc(1);
    *new_sock = socket_desc;
    if (pthread_create(&recv_thread, NULL, receive_message, (void*) new_sock) < 0) {
        perror("could not create thread");
        return 1;
    }

    //client_join(sock);
    printf("Enter username: ");
    scanf("%s",username);
    char str_join[1000];
    strcat(str_join,"2:0:3:");
    strcat(str_join,username);
    strcat(str_join,":");
    int len=strlen(str_join);
    /*
    char* str_join;
    int n1=2;
    int n2=0;
    int n3=3;
    char* c="";
    sscanf(str_join,"%d:%d:%d:%s:%s",&n1,&n2,&n3,username,c);
    int len=strlen(str_join);
    */
    printf("Buffer: %s\n",str_join);
    send(socket_desc, str_join, len, 0);
    // Keep communicating with server
    while(1) {
        printf("Enter message : ");
        scanf("%s" , message);

        //Message in SBCP format
        struct SBCP_packet msg=send_message(message);
        msg_to_char(msg);
        //strcpy(message,msg_to_char(msg));
        //printf("Message packet: %s\n",message);
        int len=sizeof(msg.attribute)+sizeof(msg.header);
        //printf("Message to be sent: %s\n",msg.attribute.payload.message);
        // Send some data
        //if (send(sock, message, strlen(message), 0) < 0) {
        if (send(socket_desc, msg_packet, strlen(msg_packet), 0) < 0) {
            puts("Send failed");
            return 1;
        }
    }
    
    close(socket_desc);
    return 0;
}
