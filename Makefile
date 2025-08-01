FILES = ./build/main.o ./build/io.o ./build/str.o ./build/hashmap.o ./build/server.o
INCLUDES = -I./src
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc -fno-stack-protector


all: ./bin/server

./bin/server: $(FILES)
	gcc $(FLAGS) -o $@ $(FILES)
#	gcc $(FLAGS) $(FILES) -o ./bin/server

./build/main.o: ./src/main.s
	as -o ./build/main.o ./src/main.s

./build/server.o: ./src/server.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/server.c -o ./build/server.o

./build/io.o: ./src/io.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/io.c -o ./build/io.o

./build/str.o: ./src/str.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/str.c -o ./build/str.o

./build/hashmap.o: ./src/hashmap.c
	gcc $(INCLUDES) $(FLAGS) -c ./src/hashmap.c -o ./build/hashmap.o

clean:
		rm -rf ./bin/server
		rm -rf ${FILES}


