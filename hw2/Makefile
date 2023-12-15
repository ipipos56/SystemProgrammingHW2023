GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

all: lib

lib: parser.c solution.c
	gcc $(GCC_FLAGS) parser.c solution.c

test: lib
	python3 checker.py

clean:
	rm *.out