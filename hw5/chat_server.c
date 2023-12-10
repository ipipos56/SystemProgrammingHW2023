#include "chat.h"
#include "chat_server.h"

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/time.h>

#include <poll.h>
#include <stdlib.h>
#include <unistd.h>


//

struct chat_peer {
    /** Client's socket. To read/write messages. */
    int socket;
    /** Output buffer. */
    char *output;
    /** Size of output buffer. */
    uint32_t output_size;
    /** Number of bytes in output buffer. */
    uint32_t output_count;
    /** Input buffer. */
    char *input;
    /** Size of input buffer. */
    uint32_t input_size;
    /** Number of bytes in input buffer. */
    uint32_t input_count;
    /* PUT HERE OTHER MEMBERS */
};

struct chat_server {
    /** Listening socket. To accept new clients. */
    int socket;
    /** Array of peers. */
    struct chat_peer *peers;
    /** Number of peers in the array. */
    uint32_t peer_count;
    /** Size of the array. */
    uint32_t peer_size;

    /** Array of received messages. */
    struct chat_message *messages;
    /** Number of messages in the array. */
    uint32_t total_messages_count;
    /** Total_messages */
    uint32_t total_messages;

    uint32_t current_message_index;
    /** Output buffer. */

    size_t is_started;


    /* PUT HERE OTHER MEMBERS */

};

struct chat_server *
chat_server_new(void) {
    struct chat_server *server = calloc(1, sizeof(*server));
    server->socket = -1;

    /* IMPLEMENT THIS FUNCTION */


    server->total_messages_count = 0;
    server->total_messages = 0;
    server->current_message_index = 0;
    server->messages = calloc(10, sizeof(struct chat_message));
    server->peer_count = 0;
    server->peer_size = 10;
    server->peers = calloc(10, sizeof(struct chat_peer));
    server->is_started = 0;





    return server;
}

void
chat_server_delete(struct chat_server *server) {
    if (server->socket >= 0)
        close(server->socket);

    /* IMPLEMENT THIS FUNCTION */


    free(server->messages);
    free(server->peers);


    free(server);
}

int
chat_server_listen(struct chat_server *server, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(port);
    /* Listen on all IPs of this machine. */
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
     * 1) Create a server socket (function socket()).
     * 2) Bind the server socket to addr (function bind()).
     * 3) Listen the server socket (function listen()).
     * 4) Create epoll/kqueue if needed.
     */
    /* IMPLEMENT THIS FUNCTION */

    server->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket < 0) {
        return CHAT_ERR_SYS;
    }

    if (bind(server->socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        return CHAT_ERR_SYS;
    }

    if (listen(server->socket, 5) < 0) {
        return CHAT_ERR_SYS;
    }

    //Create new peer
    server->peers[server->peer_count].input = calloc(1, sizeof(char));
    server->peers[server->peer_count].input_size = 1;
    server->peers[server->peer_count].input_count = 0;
    server->peers[server->peer_count].output = calloc(1, sizeof(char));
    server->peers[server->peer_count].output_size = 1;
    server->peers[server->peer_count].output_count = 0;
    server->peers[server->peer_count].socket = server->socket;


    server->peer_count += 1;

    server->is_started = 1;
    return 0;
}

struct chat_message *
chat_server_pop_next(struct chat_server *server) {
    /*
     * IMPLEMENT THIS FUNCTION - return the next message from the input
     * buffer of any client-socket. If there are no messages, then return
     * NULL.
     */

    if (server->current_message_index >= server->total_messages_count) {
        return NULL;
    }

    return &server->messages[server->current_message_index++];
}

int
chat_server_update(struct chat_server *server, double timeout) {
    /*
     * 1) Wait on epoll/kqueue/poll for update on any socket.
     * 2) Handle the update.
     * 2.1) If the update was on listen-socket, then you probably need to
     *     call accept() on it - a new client wants to join.
     * 2.2) If the update was on a client-socket, then you might want to
     *     read/write on it.
     */
    /* IMPLEMENT THIS FUNCTION */

    if(server->is_started == 0)
        return CHAT_ERR_NOT_STARTED;
    struct pollfd *pfds = calloc(server->peer_count, sizeof(struct pollfd));
    // Populate pfds with server socket and all client sockets

    // Initialize pollfd for the server socket
    pfds[server->peer_count].fd = server->socket;
    pfds[server->peer_count].events = POLLIN;

    // Initialize pollfd for each client socket
    for (uint32_t i = 0; i < server->peer_count; i++) {
        pfds[i].fd = server->peers[i].socket;
        pfds[i].events = POLLIN; // Add POLLOUT if you also want to check for writeability
    }

    // Convert timeout from seconds to milliseconds
    int timeout_ms = (int) (timeout * 1000);

    int ret = poll(pfds, server->peer_count, timeout_ms);
    if (ret < 0) {
        // Handle error
    } else if (ret == 0) {
        // Timeout occurred
        printf("TimeOut");
        free(pfds);
        return CHAT_ERR_TIMEOUT;
    }

    // Check if server socket has an event
    if (pfds[0].revents & POLLIN) {
        // Accept new client
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int client_socket = accept(server->socket, (struct sockaddr *) &addr, &addr_len);
        if (client_socket > 0) {


            // Add new client to server
            if (server->peer_count == server->peer_size) {
                server->peer_size *= 2;
                server->peers = realloc(server->peers, server->peer_size * sizeof(struct chat_peer));
            }
            server->peers[server->peer_count].socket = client_socket;
            server->peers[server->peer_count].output = NULL;
            server->peers[server->peer_count].output_size = 0;
            server->peers[server->peer_count].output_count = 0;
            server->peers[server->peer_count].input = NULL;
            server->peers[server->peer_count].input_size = 0;
            server->peers[server->peer_count].input_count = 0;
            server->peer_count++;
        }
    }

    // Check if any client socket has an event
    for (uint32_t i = 0; i < server->peer_count; i++) {
        if (pfds[i].revents & POLLIN) {
            // Read from client socket
            char buffer[1024];
            int bytes_read = read(server->peers[i].socket, buffer, 1024);
            if (bytes_read >= 0) {

                // Add bytes to input buffer
                if (server->peers[i].input_count + bytes_read > server->peers[i].input_size) {
                    server->peers[i].input_size = server->peers[i].input_count + bytes_read;
                    server->peers[i].input = realloc(server->peers[i].input, server->peers[i].input_size);
                }
                memcpy(server->peers[i].input + server->peers[i].input_count, buffer, bytes_read);
                server->peers[i].input_count += bytes_read;

                // Parse messages from input buffer
                uint32_t bytes_parsed = 0;
                while (bytes_parsed < server->peers[i].input_count) {
                    // Check if there are enough bytes to parse message length
                    if (server->peers[i].input_count - bytes_parsed < sizeof(uint32_t)) {
                        break;
                    }

                    // Parse message length
                    uint32_t message_length = *((uint32_t * )(server->peers[i].input + bytes_parsed));
                    bytes_parsed += sizeof(uint32_t);

                    // Check if there are enough bytes to parse message
                    if (server->peers[i].input_count - bytes_parsed < message_length) {
                        break;
                    }

                    // Parse message
                    struct chat_message *message = calloc(1, sizeof(struct chat_message));
                    message->peer = &server->peers[i];
                    message->length = message_length;
                    message->data = calloc(message_length, sizeof(char));
                    memcpy(message->data, server->peers[i].input + bytes_parsed, message_length);
                    bytes_parsed += message_length;

                    // Add message to server
                    if (server->total_messages == server->total_messages_count) {
                        server->total_messages *= 2;
                        server->messages = realloc(server->messages,
                                                   server->total_messages * sizeof(struct chat_message));
                    }
                    server->messages[server->total_messages_count] = *message;
                    server->total_messages_count++;
                }


                // Remove parsed bytes from input buffer
                if (bytes_parsed > 0) {
                    server->peers[i].input_count -= bytes_parsed;
                    memmove(server->peers[i].input, server->peers[i].input + bytes_parsed,
                            server->peers[i].input_count);
                }
            }
        }
    }


    free(pfds);
    return 0;
}

int
chat_server_get_descriptor(const struct chat_server *server) {
#if NEED_SERVER_FEED
    /* IMPLEMENT THIS FUNCTION if want +5 points. */

    /*
     * Server has multiple sockets - own and from connected clients. Hence
     * you can't return a socket here. But if you are using epoll/kqueue,
     * then you can return their descriptor. These descriptors can be polled
     * just like sockets and will return an event when any of their owned
     * descriptors has any events.
     *
     * For example, assume you created an epoll descriptor and added to
     * there a listen-socket and a few client-sockets. Now if you will call
     * poll() on the epoll's descriptor, then on return from poll() you can
     * be sure epoll_wait() can return something useful for some of those
     * sockets.
     */
#endif
    return server->socket;
}

int
chat_server_get_socket(const struct chat_server *server) {
    return server->socket;
}

int
chat_server_get_events(const struct chat_server *server) {
    /*
     * IMPLEMENT THIS FUNCTION - add OUTPUT event if has non-empty output
     * buffer in any of the client-sockets.
     */
    /* IMPLEMENT THIS FUNCTION */

    return server->peer_count;
}

int
chat_server_feed(struct chat_server *server, const char *msg, uint32_t msg_size) {
#if NEED_SERVER_FEED
    /* IMPLEMENT THIS FUNCTION if want +5 points. */
#endif
    (void) server;
    (void) msg;
    (void) msg_size;
    return CHAT_ERR_NOT_IMPLEMENTED;
}
