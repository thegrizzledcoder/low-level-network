#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIENTS 1024
#define BUFF_SIZE 4096
#define PORT 8080

typedef enum {
    STATE_NEW,
    STATE_CONNECTED,
    STATE_DISCONNECTED
} state_e;

typedef struct {
    int fd;
    state_e state;
    char buffer[4096];
} clientstate_t;

clientstate_t clientstates[MAX_CLIENTS];

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientstates[i].fd = -1; // -1 indicates a free slot
        clientstates[i].state = STATE_NEW;
        memset(&clientstates[i].buffer, '\0', BUFF_SIZE);
    }
}

int find_free_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientstates[i].fd == -1) {
            return i;
        }
    }
    return -1; // No free slot found
}

int find_slot_by_fd(const int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientstates[i].fd == fd) {
            return i;
        }
    }
    return -1; // Not found
}

int main(void) {
    int conn_fd;
    int listen_fd;
    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);

    struct pollfd fds[MAX_CLIENTS + 1];
    int opt = 1;

    // Initialize client states
    init_clients();

    // Create listening socket
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsocketopt");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    // Bind
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(listen_fd, 10) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    memset(fds, 0, sizeof(fds));
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    while (1) {
        int ii = 1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clientstates[i].fd != -1) {
                fds[ii].fd = clientstates[i].fd; // Offset by 1 for listen_fd
                fds[ii].events = POLLIN;
                ii++;
            }
        }

        // Wait for an event on one of the sockets
        int n_events = poll(fds, nfds, -1); // -1 means no timeout
        if (n_events == -1) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        // Check for new connections
        if (fds[0].revents & POLLIN) {
            if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection from %s:%d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            int free_slot = find_free_slot();
            if (free_slot == -1) {
                printf("Server full: closing new connection\n");
                close(conn_fd);
            } else {
                clientstates[free_slot].fd = conn_fd;
                clientstates[free_slot].state = STATE_CONNECTED;
                nfds++;
                printf("Slot %d has fd %d\n", free_slot, clientstates[free_slot].fd);
            }
            n_events--;
        }
        // Check each client for read/write activity
        for (int i = 1; i <= nfds && n_events > 0; i++) {
            if (fds[i].revents & POLLIN) {
                n_events--;

                int fd = fds[i].fd;
                int slot = find_slot_by_fd(fd);
                ssize_t bytes_read = read(fd, &clientstates[slot].buffer, sizeof(clientstates[slot].buffer));
                if (bytes_read <= 0) {
                    // Connection closed or error
                    close(fd);
                    if (slot == -1) {
                        printf("Tried to close fd that doesn't exist?\n");
                    } else {
                        clientstates[slot].fd = -1;
                        clientstates[slot].state = STATE_DISCONNECTED;
                        printf("Client disconnected or error\n");
                        nfds--;
                    }
                } else {
                    printf("Received data from client: %s\n", clientstates[slot].buffer);
                }
            }
        }
    }
}