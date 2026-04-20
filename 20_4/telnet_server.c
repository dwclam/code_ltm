#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define PORT 9000
#define MAX_CLIENTS 100

struct client_info {
    int fd;
    int logged_in;
};

int check_auth(const char *user, const char *pass) {
    FILE *f = fopen("users.txt", "r");
    if (!f) {
        perror("Khong the mo file users.txt");
        return 0;
    }
    char line[256];
    char f_user[128], f_pass[128];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%127s %127s", f_user, f_pass) == 2) {
            if (strcmp(user, f_user) == 0 && strcmp(pass, f_pass) == 0) {
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
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

    printf("Telnet server dang chay tren port %d...\n", PORT);

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
                    clients[nfds].logged_in = 0;
                    nfds++;

                    char *msg = "Vui long dang nhap (Cu phap: user pass):\n> ";
                    send(new_fd, msg, strlen(msg), 0);
                    printf("Client moi ket noi: socket %d\n", new_fd);
                } else {
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
                    if (strlen(buf) == 0) {
                        char *prompt = clients[i].logged_in ? "> " : "Vui long dang nhap (Cu phap: user pass):\n> ";
                        send(fds[i].fd, prompt, strlen(prompt), 0);
                        continue;
                    }

                    if (!clients[i].logged_in) {
                        char user[128], pass[128];
                        if (sscanf(buf, "%127s %127s", user, pass) == 2) {
                            if (check_auth(user, pass)) {
                                clients[i].logged_in = 1;
                                char *success_msg = "Dang nhap thanh cong. Nhap lenh de thuc thi:\n> ";
                                send(fds[i].fd, success_msg, strlen(success_msg), 0);
                                printf("Client socket %d dang nhap thanh cong.\n", fds[i].fd);
                            } else {
                                char *error_msg = "Sai user hoac pass. Vui long thu lai:\n> ";
                                send(fds[i].fd, error_msg, strlen(error_msg), 0);
                            }
                        } else {
                            char *format_err = "Sai cu phap. Nhap lai (user pass):\n> ";
                            send(fds[i].fd, format_err, strlen(format_err), 0);
                        }
                    } else {
                        char filename[64];
                        sprintf(filename, "out_%d.txt", fds[i].fd);

                        char cmd[2048];
                        sprintf(cmd, "%s > %s 2>&1", buf, filename);
                        system(cmd);

                        FILE *out_file = fopen(filename, "r");
                        if (out_file) {
                            char file_buf[1024];
                            int bytes_read;
                            while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), out_file)) > 0) {
                                send(fds[i].fd, file_buf, bytes_read, 0);
                            }
                            fclose(out_file);
                            remove(filename); 
                        } else {
                            char *err = "Loi thuc thi lenh hoac khong co ket qua tra ve.\n";
                            send(fds[i].fd, err, strlen(err), 0);
                        }
                        
                        char *prompt = "\n> ";
                        send(fds[i].fd, prompt, strlen(prompt), 0);
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}
