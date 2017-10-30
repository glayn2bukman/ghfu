# run 
#   $ make libghfu && make main

# the .so lib
libghfu: data.h ghfu.h ghfu.c
	gcc -shared -fPIC -o libghfu.so ghfu.c

# the main(for testing the library)
main: data.h ghfu.h ghfu.c main.c
	gcc -Wall -g -o main main.c ghfu.c

# install
