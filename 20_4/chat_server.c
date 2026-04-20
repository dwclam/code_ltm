#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>

#define PORT 9000
#define MAX_CLIENTS 100

struct client_info {
    int fd;
    int registered;
    char id[32];
    char name[64];
};

void get_current_time(char *time_str, size_t max_size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(time_str, max_size, "%Y/%m/%d %I:%M:%S%p", tm_info);
}

int main() {
    int listener, new_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("Loi tao socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Loi bind");
        exit(1);
    }

    if (listen(listener, 10) < 0) {
        perror("Loi listen");
        exit(1);
    }

    printf("Chat server dang chay tren port %d...\n", PORT);

    struct pollfd fds[MAX_CLIENTS + 1];
    struct client_info clients[MAX_CLIENTS + 1];
    int nfds = 1;

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    char buf[1024];

    while (1) {
        int poll_count = poll(fds, nfds, -1);
        if (poll_count < 0) {
            perror("Loi poll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            new_fd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);
            if (new_fd < 0) {
                perror("Loi accept");
            } else {
                if (nfds <= MAX_CLIENTS) {
                    fds[nfds].fd = new_fd;
                    fds[nfds].events = POLLIN;

                    clients[nfds].fd = new_fd;
                    clients[nfds].registered = 0;
                    memset(clients[nfds].id, 0, sizeof(clients[nfds].id));
                    memset(clients[nfds].name, 0, sizeof(clients[nfds].name));

                    nfds++;

                    char *ask_name = "Vui long nhap theo cu phap -> client_id: client_name\n";
                    send(new_fd, ask_name, strlen(ask_name), 0);
                    printf("Client moi ket noi: socket %d\n", new_fd);
                } else {
                    printf("Tu choi ket noi. Server da day!\n");
                    close(new_fd);
                }
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                memset(buf, 0, sizeof(buf));
                int bytes_received = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

                if (bytes_received <= 0) {
                    printf("Client socket %d ngat ket noi.\n", fds[i].fd);
                    close(fds[i].fd);

                    fds[i] = fds[nfds - 1];
                    clients[i] = clients[nfds - 1];
                    nfds--;
                    i--;
                } else {
                    buf[strcspn(buf, "\r\n")] = 0;
                    
                    if (strlen(buf) == 0) continue;

                    if (!clients[i].registered) {
                        char id_temp[32];
                        char name_temp[64];

                        if (sscanf(buf, "%31[^:]: %63s", id_temp, name_temp) == 2) {
                            strcpy(clients[i].id, id_temp);
                            strcpy(clients[i].name, name_temp);
                            clients[i].registered = 1;

                            char success_msg[256];
                            sprintf(success_msg, "Dang ky thanh cong! Xin chao %s (ID: %s)\n", name_temp, id_temp);
                            send(fds[i].fd, success_msg, strlen(success_msg), 0);
                            printf("Client socket %d dang ky thanh cong -> ID: %s, Name: %s\n", fds[i].fd, id_temp, name_temp);
                        } else {
                            char *error_msg = "Sai cu phap. Vui long nhap lai -> client_id: client_name\n";
                            send(fds[i].fd, error_msg, strlen(error_msg), 0);
                        }
                    } else {
                        char time_str[64];
                        get_current_time(time_str, sizeof(time_str));

                        char send_buf[2048];
                        sprintf(send_buf, "%s %s: %s\n", time_str, clients[i].id, buf);

                        for (int j = 1; j < nfds; j++) {
                            if (i != j && clients[j].registered) {
                                send(fds[j].fd, send_buf, strlen(send_buf), 0);
                            }
                        }
                        printf("Da forward tin nhan tu [%s]: %s\n", clients[i].id, buf);
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}
