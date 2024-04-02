#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>

typedef enum {
    PROTO_HELLO,
} proto_type_e;

// TLV (Type, Length, Value) header
typedef struct {
    proto_type_e type;
    unsigned short len;
} proto_hdr_t;

void handle_client(int fd);

int main(void) {
    struct sockaddr_in serverInfo = {0};
    struct sockaddr_in clientInfo = {0};
    socklen_t clientSize = 0;

    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(5555);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    printf("Socket created\n");

    // bind to a port
    if (bind(fd, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) == -1) {
        perror("bind");
        close(fd);
        return -1;
    }
    // listen for incoming connections
    if (listen(fd, 0) == -1) {
        perror("listen");
        close(fd);
        return -1;
    }
    // accept incoming connections
    while(1) {
        int cfd = accept(fd, (struct sockaddr *) &clientInfo, &clientSize);
        if (cfd == -1) {
            perror("accept");
            close(fd);
            return -1;
        }

        handle_client(cfd);

        close(cfd);
    }
    return 0;
}

void handle_client(int fd) {
    char buf[4096] = {0};
    proto_hdr_t *hdr = buf;
    hdr->type = htonl(PROTO_HELLO);
    hdr->len = htons(sizeof(int));
    int reallen = hdr->len;
    hdr->len = htons(hdr->len);

    int *data = (int*)&hdr[1];
    *data = htonl(1);

    write(fd, hdr, sizeof(proto_hdr_t) + reallen);
}
