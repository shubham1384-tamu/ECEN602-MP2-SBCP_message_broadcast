#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define PORT 12345

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
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char send_buffer[1024];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Send JOIN message
    SBCP_Message join_message;
    join_message.header.type = MSG_JOIN;
    join_message.header.length = sizeof(SBCP_Message);
    strcpy(join_message.attributes[0].payload, "Username");
    send(sock, &join_message, sizeof(join_message), 0);

    // Listen for incoming messages
    while((valread = read(sock, send_buffer, sizeof(send_buffer))) > 0) {
        send_buffer[valread] = '\0';
        printf("Received: %s\n", send_buffer);
    }

    return 0;
}
