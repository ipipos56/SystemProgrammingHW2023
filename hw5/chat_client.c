#include "chat.h"
#include "chat_client.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <sys/types.h>
//#include <sys/event.h>
#include <sys/time.h>

#include <poll.h>
#include <errno.h>

#include <ctype.h>


struct chat_client {
    /** Socket connected to the server. */
    int socket;
    /** Array of received messages. */
    struct chat_message *messages;
    /** Number of messages in the array. */
    uint32_t total_messages_count;
    uint32_t current_message_index;
//    /** Output buffer. */
//    struct chat_message *outgoing_messages;
//    /** Number of messages in the array. */
//    uint32_t outgoing_messages_count;

    uint32_t cur_event;

    /** Name of this client. */
    char *name;

    int is_started;
};

void _split_addr_port(const char *addr_port, char *addr, size_t addr_size, char *port, size_t port_size) {
    const char *colon = strchr(addr_port, ':');
    if (colon == NULL) {
        fprintf(stderr, "Invalid address format\n");
        exit(1);
    }

    size_t addr_length = (size_t)(colon - addr_port);
    size_t port_length = (size_t)(strlen(addr_port) - addr_length - 1);

    if (addr_length >= addr_size || port_length >= port_size) {
        fprintf(stderr, "Buffer size too small\n");
        exit(1);
    }

    strncpy(addr, addr_port, addr_length);
    addr[addr_length] = '\0'; // Null-terminate the string

    strncpy(port, colon + 1, port_length);
    port[port_length] = '\0'; // Null-terminate the string
}


void _process_buffer(char *buffer, ssize_t bytes_read) {
    static char message_buffer[BUFFER_SIZE];
    static int message_buffer_length = 0;

    for (int i = 0; i < bytes_read; ++i) {
        if (buffer[i] == '\n') {
            message_buffer[message_buffer_length] = '\0';

            // Trim spaces from the message
            char *start = message_buffer;
            char *end = message_buffer + message_buffer_length - 1;
            while (isspace((unsigned char) *start)) start++;
            while (end > start && isspace((unsigned char) *end)) end--;
            *(end + 1) = '\0';

            if (start != end) { // Check if message is not just spaces
                // Store the trimmed message in the client's message queue
                // enqueue_message(client, start);
            }

            message_buffer_length = 0; // Reset for the next message
        } else {
            if (message_buffer_length < BUFFER_SIZE - 1) {
                message_buffer[message_buffer_length++] = buffer[i];
            }
            // If message is too long, you might want to handle this case
        }
    }
}



struct chat_client *
chat_client_new(const char *name) {
    /* Ignore 'name' param if don't want to support it for +5 points. */

    struct chat_client *client = calloc(1, sizeof(*client));
    client->socket = -1;
    client->cur_event = 0;

    client->total_messages_count = 10;
    client->messages = calloc(client->total_messages_count, sizeof(struct chat_message));
    client->current_message_index = 0;

    //client->outgoing_messages = calloc(1, sizeof(struct chat_message));
    //client->outgoing_messages_count = 0;

    //client->events = calloc(1, sizeof(struct epoll_event));

    client->name = calloc(1, sizeof(char) * (strlen(name) + 1));
    strcpy(client->name, name);

    client->is_started = 0;


    /* IMPLEMENT THIS FUNCTION */

    return client;
}

void
chat_client_delete(struct chat_client *client) {
    if (client != NULL) {
        free(client->name);

        if (client->socket >= 0)
            close(client->socket);

        /* IMPLEMENT THIS FUNCTION */

        free(client);
    }
}

int
chat_client_connect(struct chat_client *client, const char *addr) {
    /*
     * 1) Use getaddrinfo() to resolve addr to struct sockaddr_in.
     * 2) Create a client socket (function socket()).
     * 3) Connect it by the found address (function connect()).
     */
    /* IMPLEMENT THIS FUNCTION */

    char ip[16]; // Size for IPv4 address
    char port[6];  // Size for port number

    _split_addr_port(addr, ip, sizeof(ip), port, sizeof(port));


    struct addrinfo hints, *res;
    int sockfd;

    // Setup hints for getaddrinfo
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;

    // Resolve the address
    if (getaddrinfo(ip, port, &hints, &res) != 0) {
        return CHAT_ERR_NOT_IMPLEMENTED; // Replace "PORT" with actual port number
    }

    // Create a socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        printf("sockfd");
        freeaddrinfo(res);
        return CHAT_ERR_NOT_IMPLEMENTED;
    }

    // Set socket to non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        printf("fcntl1");
        perror("fcntl");
        close(sockfd);
        return CHAT_ERR_NOT_IMPLEMENTED;
    }
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) < 0) {
        printf("fcntl2");
        perror("fcntl");
        close(sockfd);
        return CHAT_ERR_NOT_IMPLEMENTED;
    }

    // Connect the socket
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        if (errno != EINPROGRESS) { // Handle real error
            perror("connect");
            close(sockfd);
            freeaddrinfo(res);
            return CHAT_ERR_NOT_IMPLEMENTED;
        }
        // If errno is EINPROGRESS, proceed with the connection setup
    }

    // Store the sockfd in the client structure
    client->socket = sockfd;

    freeaddrinfo(res);
    client->is_started = 1;
    return 0; // Success

}

struct chat_message *
chat_client_pop_next(struct chat_client *client) {
    if (client == NULL || client->total_messages_count <= 0 ||
        client->current_message_index >= client->total_messages_count) {
        return NULL;
    }

    // Get the current message
    struct chat_message *message = &client->messages[client->current_message_index];

    // Increment the index for the next call
    client->current_message_index++;

    return message;
}

int
chat_client_update(struct chat_client *client, double timeout) {
    /*
     * The easiest way to wait for updates on a single socket with a timeout
     * is to use poll(). Epoll is good for many sockets, poll is good for a
     * few.
     *
     * You create one struct pollfd, fill it, call poll() on it, handle the
     * events (do read/write).
     */
    if (client->is_started == 0)
        return CHAT_ERR_NOT_STARTED;
    if (client == NULL || client->socket < 0) {
        return CHAT_ERR_SYS; // Invalid client or socket
    }

    struct pollfd fds;
    fds.fd = client->socket;
    fds.events = POLLIN | POLLOUT; // Interested in read and write readiness
    fds.revents = 0;

    int timeout_ms = (int) (timeout * 1000);

    int ret = poll(&fds, 1, timeout_ms);
    if (ret < 0) {
        perror("poll");
        return -1; // Poll error
    } else if (ret == 0) {
        return 0; // Timeout, no events
    }

    char read_buffer[BUFFER_SIZE];
    char write_buffer[BUFFER_SIZE]; // Buffer for outgoing messages
    ssize_t bytes_read, bytes_written;

    if (fds.revents & POLLIN) {
        // Read data from the socket
        bytes_read = read(client->socket, read_buffer, sizeof(read_buffer) - 1);
        if (bytes_read > 0) {
            read_buffer[bytes_read] = '\0'; // Null-terminate the string
            _process_buffer(read_buffer, bytes_read);
        } else if (bytes_read < 0 && errno != EWOULDBLOCK) {
            perror("read");
            // Handle read error
        }
    }

    if (fds.revents & POLLOUT) {
        // Prepare data to be written (get from your outgoing message queue)
        // For example: snprintf(write_buffer, BUFFER_SIZE, "%s", "Your message");

        bytes_written = write(client->socket, write_buffer, strlen(write_buffer));
        if (bytes_written < 0 && errno != EWOULDBLOCK) {
            perror("write");
            // Handle write error
        }
        // Handle partial writes by adjusting the position in your outgoing buffer
    }

    return 0;
}


int
chat_client_get_descriptor(const struct chat_client *client) {
    return client->socket;
}

int
chat_client_get_events(const struct chat_client *client) {
    /*
     * IMPLEMENT THIS FUNCTION - add OUTPUT event if has non-empty output
     * buffer.
     */
    return client->cur_event;
}

int
chat_client_feed(struct chat_client *client, const char *msg, uint32_t msg_size) {
    if (client == NULL || msg == NULL || msg_size == 0) {
        return -1; // Invalid arguments
    }

    if (client->total_messages_count <= client->current_message_index) {
        return -1; // No more space
    }

    // Copy the message
    struct chat_message *message = &client->messages[client->total_messages_count];
    message->length = msg_size;
    message->data = malloc(msg_size);
    memcpy(message->data, msg, msg_size);
    printf("%s", message->data);

    // Increment the total messages count
    client->current_message_index++;

    return 0;
}