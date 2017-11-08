# run 
#   $ make && make main
#		OR
#   $ make libghfu && make main

# the .so lib
libghfu: data.h ghfu.h ghfu.c
	# would have linked against pthread (-lpthread) if we had threads in the library. however, we only
	# have thread-safe code(mutex) but dont generate threads within libjermGHFU.so itself(the threads are
	# generated in the server where libjermGHFU.so is invoked)
	mkdir -p lib && gcc -O3 -shared -fPIC -Wall -o lib/libjermGHFU.so ghfu.c -ldl

# the main(for testing the library)
main: data.h ghfu.h ghfu.c main.c
	gcc -Wall -g -o main main.c ghfu.c -ldl

all: data.h ghfu.h ghfu.c main.c
	make libghfu && make main

# install
