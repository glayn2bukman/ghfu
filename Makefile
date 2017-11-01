# run 
#   $ make libghfu && make main

# the .so lib
libghfu: data.h ghfu.h ghfu.c
	mkdir -p lib && gcc -shared -fPIC -Wall -o lib/libjermGHFU.so ghfu.c

# the main(for testing the library)
main: data.h ghfu.h ghfu.c main.c
	gcc -Wall -g -o main main.c ghfu.c

all: data.h ghfu.h ghfu.c main.c
	make libghfu && make main

# install
