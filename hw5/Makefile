GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

all: lib exe test

lib: chat.c chat_client.c chat_server.c
	gcc $(GCC_FLAGS) -c chat.c -o chat.o -g
	gcc $(GCC_FLAGS) -c chat_client.c -o chat_client.o -g
	gcc $(GCC_FLAGS) -c chat_server.c -o chat_server.o -g

exe: lib chat_client_exe.c chat_server_exe.c
	gcc $(GCC_FLAGS) chat_client_exe.c chat.o chat_client.o -o client -g
	gcc $(GCC_FLAGS) chat_server_exe.c chat.o chat_server.o -o server -g

test: lib
	gcc $(GCC_FLAGS) test.c chat.o chat_client.o chat_server.o -o test 	\
		-I ../utils -lpthread -g

clean:
	rm *.o
	rm client server test
