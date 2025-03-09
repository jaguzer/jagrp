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

void forward_request (int client_sock) {
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

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_sock, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        write(backend_sock, buffer, bytes_read);
    
        // forward backend reponse to client
        while ((bytes_read = read(backend_sock, buffer, BUFFER_SIZE - 1))) {
            write(client_sock, buffer, bytes_read);
        }
    }
    
    close(backend_sock);
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {

    }
}