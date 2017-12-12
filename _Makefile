# run 
#   $ make && make main
#		OR
#   $ make libghfu && make main

CC = gcc#clang
CFLAGS = -Wall -Wextra
CLIBFLAGS = -O3 -shared -fPIC -Wall -Wextra

# the .so lib
libghfu: data.h ghfu.h ghfu.c lib/libjermCrypt.so
	# would have linked against pthread (-lpthread) if we had threads in the library. however, we only
	# have thread-safe code(mutex) but dont generate threads within libjermGHFU.so itself(the threads are
	# generated in the server where libjermGHFU.so is invoked)
	mkdir -p lib && $(CC) $(CLIBFLAGS) -o lib/libjermGHFU.so ghfu.c -ldl && sudo mv lib/libjermGHFU.so /usr/lib

# the main(for testing the library)
main: data.h ghfu.h ghfu.c main.c lib/libjermCrypt.so
	$(CC) $(CFLAGS) -o main main.c -ljermGHFU

all: data.h ghfu.h ghfu.c main.c lib/libjermCrypt.so
	make libghfu && make main

# install
