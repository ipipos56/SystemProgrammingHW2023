#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define MAX_BUFFER 49152 // Maximum buffer size (for 48KB packs)
#define NUM_RUNS 5

double timediff(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

void make_socket_non_blocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        exit(EXIT_FAILURE);
    }
}

void server(int pack_size, long total_size) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER] = {0};
    long total_received = 0;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    // Bind the socket to the server address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    make_socket_non_blocking(new_socket);



    while (total_received < total_size) {
        ssize_t bytes_received = recv(new_socket, buffer, pack_size, 0);
        if (bytes_received > 0) {
            total_received += bytes_received;
        }
    }

    close(new_socket);
    close(server_fd);
}

void client(int pack_size, long total_size) {

    struct sockaddr_in serv_addr;
    int sock;
    long total_sent = 0;
    char buffer[pack_size];

    memset(&serv_addr, '0', sizeof(serv_addr));

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        exit(EXIT_FAILURE);
    }

    //Trying to connect to server until connection established
    while (1) {
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Socket creation error \n");
            exit(EXIT_FAILURE);
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0) {
            break;
        }

        close(sock); // Close the socket and try again
    }

    make_socket_non_blocking(sock);

    // Fill buffer with data
    memset(buffer, 'A', pack_size);

    while (total_sent < total_size) {
        ssize_t bytes_sent = send(sock, buffer, pack_size, 0);
        if (bytes_sent > 0) {
            total_sent += bytes_sent;
        }
    }

    close(sock);
}

int cmp(const void *a, const void *b) {
    return (*(double*)a - *(double*)b);
}

void calculate_stats(double durations[], int n, double *min, double *max, double *median) {
    *min = *max = durations[0];
    for (int i = 1; i < n; i++) {
        if (durations[i] < *min) *min = durations[i];
        if (durations[i] > *max) *max = durations[i];
    }

    // For median, sort the array and find the middle value
    qsort(durations, n, sizeof(double), cmp);
    if (n % 2 == 0) {
        *median = (durations[n / 2 - 1] + durations[n / 2]) / 2;
    } else {
        *median = durations[n / 2];
    }
}


int main() {
    int pack_sizes[] = {16384, 1024, 49152, 512}; // 16KB, 1KB, 48KB, 512B
    long total_size = 15L * 1024 * 1024 * 1024; // 15GB


    for (unsigned long i = 0; i < sizeof(pack_sizes) / sizeof(int); i++) {
        int pack_size = pack_sizes[i];
        double *durations = malloc(NUM_RUNS * sizeof(double));

        for (int run = 0; run < NUM_RUNS; run++) {
            struct timespec start_time, end_time;
            clock_gettime(CLOCK_MONOTONIC, &start_time);

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) {
                // Child process runs the server
                server(pack_size, total_size);
                exit(0);
            } else {
                // Parent process runs the client
                client(pack_size, total_size);
                // Wait for child process to finish
                wait(NULL);
            }


            clock_gettime(CLOCK_MONOTONIC, &end_time);
            durations[run] = timediff(start_time, end_time);
            //printf("%f", timediff(start_time, end_time));
        }

        double min, max, median;
        calculate_stats(durations, NUM_RUNS, &min, &max, &median);
        printf("Pack size: %d bytes, Min: %f, Max: %f, Median: %f seconds\n", pack_size, min, max, median);
        free(durations); // Free the allocated memory
    }

    return 0;
}