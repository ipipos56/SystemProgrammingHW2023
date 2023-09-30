cmake_minimum_required(VERSION 3.25)
project(SPHomework C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -Werror -Wall -Wno-gnu-folding-constant")

add_executable(SPHomework libcoro.c solution.c)
