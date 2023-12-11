GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

all: main1 main3 main4 main5

main1: 1/main1.c
	gcc $(GCC_FLAGS) 1/main1.c -o 1/main1 -g
	chmod -x 1/main1
	./1/main1

main3: 3/main3.c
	gcc $(GCC_FLAGS) 3/main3.c -o 3/main3 -g
	chmod -x 3/main3
	./3/main3 1
	./3/main3 2
	./3/main3 3

main4: 4/main4.c
	gcc $(GCC_FLAGS) 4/main4.c -o 4/main4 -g
	chmod -x 4/main4
	./4/main4

main5: 5/main5.c
	gcc $(GCC_FLAGS) 5/main5.c -o 5/main5 -g
	chmod -x 5/main5
	./5/main5

main7: 7/main7.c
	gcc $(GCC_FLAGS) 7/main7.c -o 7/main7 -g
	chmod -x 7/main7
	./7/main7

clean:
	rm -f 1/main1 2/main2 3/main3 4/main4 5/main5 6/main6 7/main7