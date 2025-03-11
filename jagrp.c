#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 80
#define route_IP "127.0.0.1"
#define BUFFER_SIZE 4096
#define CONFIG_FILE "routes.conf"

struct route {
    char* host;
    char* ip;
    int port;
};

struct route* routes = NULL;
int num_routes = 0;

// read routes.conf for routing table
void load_routing_table() {
    printf("Loading routes.conf.\n");
    FILE* file = fopen(CONFIG_FILE, "r");
    if (!file)
{
    perror("Failed to open config file");
    exit(1);
}

    char line[256];
    num_routes = 0;
    while (fgets(line, sizeof(line, file), file)) {
        if (line[0] == '\n' || line['#'] == '#') continue;
        int port;
        char host[128], ip[16];
        if (sscanf(line, "%127s %15s %d", host, ip, &port) != 3) {
            printf("Invalid line in routes.conf: %s", line);
            continue;
        }

        routes = realloc(routes, (num_routes + 1) * sizeof(struct route));
        routes[num_routes].host = strdup(host);
        routes[num_routes].ip = strdup(ip);
        routes[num_routes].port = port;
        num_routes++;
    }
    fclose(file);
    printf("Loaded %d routes from %s\n", num_routes, CONFIG_FILE);
}

// find route based on HTTP host header
struct route* get_route(const char* host_header) {
    for (int i = 0; routes[i].host != NULL; i++) {
        if (strcasestr(host_header, routes[i].host) != NULL) {
            return &routes[i];
        }
    }

    return &routes[0];
}

// forward request, receive response
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
    struct route* target = &routes[0]; // default route
    if (host_line) {
        char host_value[256] = {0};
        sscanf(host_line, "Host: %255[^\r\n]", host_value);
        target = get_route(host_value);
        printf("Routing to %s (%s:%d)\n", host_value, target->ip, target->port);
    } else {
        printf("No Host header found, using default route\n");
    }

    int route_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (route_sock < 0) {
        perror("route socket creation failed");
        return;
    }

    struct sockaddr_in route_addr = {0};
    route_addr.sin_family = AF_INET;
    route_addr.sin_port = htons(target->port);
    inet_pton(AF_INET, target->ip, &route_addr.sin_addr);

    if (connect(route_sock, (struct sockaddr*)&route_addr, sizeof(route_addr)) < 0) {
        perror("route connection failed");
        close(route_sock);
        return;
    }

    // forward request to route
    write(route_sock, buffer, bytes_read);
    while((bytes_read = read(route_sock, buffer, BUFFER_SIZE - 1)) > 0) {
        write(client_sock, buffer, bytes_read);
    }
    
    close(route_sock);
}

int main() {
    load_routing_table();

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