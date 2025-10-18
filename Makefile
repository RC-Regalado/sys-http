FILES = ./build/main.o ./build/io.o ./build/str.o ./build/hashmap.o ./build/mmap.o ./build/memory.o ./build/server.o ./build/requests.o
INCLUDES = -I./src
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc -fno-stack-protector

all: ./bin/server

./bin/server: $(FILES)
	gcc $(FLAGS) -o $@ $(FILES)
#	gcc $(FLAGS) $(FILES) -o ./bin/server

./build/mmap.o: ./src/asm/mmap.s
	as -o ./build/mmap.o ./src/asm/mmap.s

./build/main.o: ./src/asm/main.s
	as -o ./build/main.o ./src/asm/main.s

./build/printf.o: ./src/asm/printf.s
	as -o ./build/printf.o ./src/asm/printf.s

./build/requests.o: ./src/requests.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/requests.c -o ./build/requests.o

./build/server.o: ./src/server.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/server.c -o ./build/server.o

./build/memory.o: ./src/memory.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/memory.c -o ./build/memory.o

./build/io.o: ./src/io.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/io.c -o ./build/io.o

./build/str.o: ./src/str.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/str.c -o ./build/str.o

./build/hashmap.o: ./src/hashmap.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/hashmap.c -o ./build/hashmap.o

clean:
		rm -rf ./bin/server
		rm -rf ${FILES}


