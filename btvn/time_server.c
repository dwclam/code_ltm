#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 9000

// Hàm xử lý từng client riêng biệt
void handle_client(int client_fd) {
    char buf[256];
    char response[256];

    char *welcome_msg = "Da ket noi toi Time Server.\nCu phap: GET_TIME [format]\nCac format ho tro: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy\n> ";
    send(client_fd, welcome_msg, strlen(welcome_msg), 0);

    while (1) {
        memset(buf, 0, sizeof(buf));
        int bytes_received = recv(client_fd, buf, sizeof(buf) - 1, 0);

        // Client ngắt kết nối
        if (bytes_received <= 0) {
            printf("Client (PID: %d) da ngat ket noi.\n", getpid());
            break;
        }

        // Cắt bỏ ký tự xuống dòng
        buf[strcspn(buf, "\r\n")] = 0;

        // Bỏ qua nếu client chỉ bấm Enter
        if (strlen(buf) == 0) {
            send(client_fd, "> ", 2, 0);
            continue;
        }

        // Kiểm tra tính đúng đắn của lệnh: Phải bắt đầu bằng "GET_TIME "
        if (strncmp(buf, "GET_TIME ", 9) == 0) {
            char *format = buf + 9; // Lấy phần định dạng sau chữ "GET_TIME "

            // Lấy thời gian hiện tại của hệ thống
            time_t t = time(NULL);
            struct tm *tm_info = localtime(&t);
            memset(response, 0, sizeof(response));

            // So sánh các format được hỗ trợ
            if (strcmp(format, "dd/mm/yyyy") == 0) {
                strftime(response, sizeof(response), "%d/%m/%Y\n", tm_info);
            }
            else if (strcmp(format, "dd/mm/yy") == 0) {
                strftime(response, sizeof(response), "%d/%m/%y\n", tm_info);
            }
            else if (strcmp(format, "mm/dd/yyyy") == 0) {
                strftime(response, sizeof(response), "%m/%d/%Y\n", tm_info);
            }
            else if (strcmp(format, "mm/dd/yy") == 0) {
                strftime(response, sizeof(response), "%m/%d/%y\n", tm_info);
            }
            else {
                // Sai định dạng format
                strcpy(response, "Loi: Format khong duoc ho tro. Vui long chon 1 trong 4 format tren.\n");
            }

            send(client_fd, response, strlen(response), 0);
        } else {
            // Sai cú pháp lệnh ban đầu
            char *err_msg = "Loi: Sai cu phap lenh. Vui long su dung: GET_TIME [format]\n";
            send(client_fd, err_msg, strlen(err_msg), 0);
        }

        send(client_fd, "> ", 2, 0);
    }

    close(client_fd);
    exit(0); // Kết thúc tiến trình con
}

int main() {
    int listener, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // Bỏ qua tín hiệu SIGCHLD để tránh tạo zombie process khi tiến trình con chết
    signal(SIGCHLD, SIG_IGN);

    // Tạo socket
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("Loi tao socket");
        exit(1);
    }

    // Tùy chọn để có thể bind lại port ngay lập tức
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind và Listen
    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Loi bind");
        exit(1);
    }

    if (listen(listener, 10) < 0) {
        perror("Loi listen");
        exit(1);
    }

    printf("Time Server (Multiprocessing) dang chay tren port %d...\n", PORT);

    // Vòng lặp chờ kết nối (Multiprocessing)
    while (1) {
        client_fd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("Loi accept");
            continue;
        }

        printf("Co client moi ket noi!\n");

        // Sử dụng fork() để tạo tiến trình con
        pid_t pid = fork();

        if (pid < 0) {
            perror("Loi fork");
            close(client_fd);
        } else if (pid == 0) {
            // Trong tiến trình con: Đóng socket lắng nghe và xử lý client
            close(listener);
            handle_client(client_fd);
        } else {
            // Trong tiến trình cha: Đóng socket client vì con đã xử lý
            close(client_fd);
        }
    }

    close(listener);
    return 0;
}
