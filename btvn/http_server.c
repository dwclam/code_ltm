#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define PORT 8080
#define PREFORK_CHILDREN 4

void handle_client(int client) {
    char buf[2048];
    int ret = recv(client, buf, sizeof(buf) - 1, 0);

    if (ret > 0) {
        buf[ret] = 0;
        printf("\n--- [Worker PID: %d] Nhan request ---\n", getpid());
        puts(buf);

        char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Xin chao cac ban</h1></body></html>";
        send(client, msg, strlen(msg), 0);
    }

    close(client);
}

void worker_process(int listener) {
    printf("Worker PID %d da khoi dong va dang cho ket noi...\n", getpid());

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            perror("Loi accept");
            continue;
        }

        handle_client(client);
    }
}

int main() {
    int listener;
    struct sockaddr_in server_addr;

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

    printf("HTTP Server (Preforking mode) dang chay tren port %d...\n", PORT);

    // BƯỚC PREFORKING: Tạo sẵn N tiến trình con
    for (int i = 0; i < PREFORK_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Tiến trình con sẽ rơi vào vòng lặp vô hạn ở hàm này
            worker_process(listener);
            exit(0);
        } else if (pid < 0) {
            perror("Loi fork");
        }
    }

    // Tiến trình cha không làm gì ngoài việc chờ các tiến trình con (nếu chúng bị kill)
    while (wait(NULL) > 0);

    close(listener);
    return 0;
}
