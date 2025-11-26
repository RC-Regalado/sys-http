FILES = ./build/main.o ./build/io.o ./build/str.o ./build/hashmap.o ./build/mmap.o ./build/memory.o ./build/server.o ./build/requests.o ./build/epoll.o ./build/epoll_loop.o ./build/client.o
INCLUDES = -I./src -Isrc/includes
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -fno-stack-protector

all: ./bin/server

./bin/server: $(FILES)
	gcc $(FLAGS) -o $@ $(FILES)

build/%.o: src/asm/%.s
	as -o $@ $<

build/%.o: src/%.c
	gcc $(INCLUDES) $(FLAGS) -c $< -o $@

clean:
		@rm -rf ./bin/server
		@rm -rf ${FILES}


