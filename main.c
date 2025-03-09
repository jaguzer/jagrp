#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 80
#define BACKEND_IP "127.0.0.1"
#define BACKEND_PORT 3000
#define BUFFER_SIZE 1024

struct backend {
    char* host;
    char* ip;
    int port;
};

struct backend backends[] = {
    {"app1.local", "127.0.0.1", 3000},
    {"app2.local", "127.0.0.1", 3001},
    {NULL, NULL, 0}
};

struct backend* get_backend(const char* host_header) {
    for (int i = 0; backends[i].host != NULL; i++) {
        if (strcasestr(host_header, backends[i].host) != NULL) {
            return &backends[i];
        }
    }

    return &backends[0];
}

void forward_request (int client_sock) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_sock, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        perror("Failed to read client request");
        return;
    }
    buffer[bytes_read] = '\0';

    // parse http request for host header
    char * host_line = strstr(buffer, "Host: ");
    struct backend* target = &backends[0]; // default backend
    if (host_line) {
        char host_value[256] = {0};
        sscanf(host_line, "Host: %255[^\r\n]", host_value);
        target = get_backend(host_value);
        printf("Routing to %s (%s:%d)\n", host_value, target->ip, target->port);
    } else {
        printf("No Host header found, using default backend\n");
    }

    int backend_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_sock < 0) {
        perror("Backend socket creation failed");
        return;
    }

    struct sockaddr_in backend_addr = {0};
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(BACKEND_PORT);
    inet_pton(AF_INET, BACKEND_IP, &backend_addr.sin_addr);

    if (connect(backend_sock, (struct sockaddr*)&backend_addr, sizeof(backend_addr)) < 0) {
        perror("Backend connection failed");
        close(backend_sock);
        return;
    }

    // forward request to backend
    write(backend_sock, buffer, bytes_read);
    while((bytes_read = read(backend_sock, buffer, BUFFER_SIZE - 1)) > 0) {
        write(client_sock, buffer, bytes_read);
    }
    
    close(backend_sock);
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Reverse proxy listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);

        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        forward_request(client_sock);
        close(client_sock);
    }

    close(server_sock);
    return 0;
}