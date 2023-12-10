#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "chat_server.h"



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


#pragma once

enum chat_errcode {
	CHAT_ERR_INVALID_ARGUMENT = 1,
	CHAT_ERR_TIMEOUT,
	CHAT_ERR_PORT_BUSY,
	CHAT_ERR_NO_ADDR,
	CHAT_ERR_ALREADY_STARTED,
	CHAT_ERR_NOT_IMPLEMENTED,
	CHAT_ERR_NOT_STARTED,
	CHAT_ERR_SYS,
};

enum chat_events {
	CHAT_EVENT_INPUT = 1,
	CHAT_EVENT_OUTPUT = 2,
};

struct chat_message {
#if NEED_AUTHOR
	/** Author's name. */
	const char *author;
#endif
	/** 0-terminate text. */
	char *data;
    /** peer address */
    struct chat_peer *peer;
    /** length of message */
    size_t length;
};

/** Free message's memory. */
void
chat_message_delete(struct chat_message *msg);

/** Convert chat_events mask to events suitable for poll(). */
int
chat_events_to_poll_events(int mask);
