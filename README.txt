to compile the main file along with the ghfu.c file, run
    $ gcc -Wall -g -o main main.c ghfu.c && valgrind --leak-check=full ./main
    #valgrind will help show any memory leaks in the program (YOU DONT WANT THIS TO HAPPEN)
