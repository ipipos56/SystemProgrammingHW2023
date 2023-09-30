
#!/bin/bash

# Generate the test files using generator.py
python3 generator.py -f test1.txt -c 10000 -m 10000
python3 generator.py -f test2.txt -c 10000 -m 10000
python3 generator.py -f test3.txt -c 10000 -m 10000
python3 generator.py -f test4.txt -c 10000 -m 10000
python3 generator.py -f test5.txt -c 10000 -m 10000
python3 generator.py -f test6.txt -c 100000 -m 10000

# Compile the solution
gcc libcoro.c solution.c -o main

# Run the solution
./main 100 3 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt

# Check the result
python3 checker.py -f output.txt
