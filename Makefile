FILES = ./build/main.o ./build/io.o ./build/str.o ./build/hashmap.o ./build/mmap.o ./build/memory.o ./build/server.o ./build/requests.o ./build/epoll_loop.o ./build/client.o ./build/files.o ./build/syscalls.o ./build/json.o ./build/database.o
INCLUDES = -I./src -Isrc/includes -Imicrodb/src/include/
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -fno-stack-protector -fvisibility=hidden
MICROLIB = ./bin/libmicrodb.so

all: ./bin/server

test: ./bin/server
	sh scripts/http_smoke.sh

./bin/server: $(MICROLIB) $(FILES)
	gcc $(FLAGS) -L./bin -lmicrodb -o $@ $(FILES)
	cp -r templates/ bin/

build/%.o: src/asm/%.s
	as -o $@ $<

build/%.o: src/%.c
	gcc $(INCLUDES) $(FLAGS) -c $< -o $@

clean:
		@rm ./bin/server
		@rm $(MICROLIB)
		@rm -rf ${FILES}
		@$(MAKE) -C microdb clean

.PHONY: microdb-lib
microdb-lib:
		@$(MAKE) -C microdb

$(MICROLIB): microdb-lib
		@cp microdb/bin/libmicrodb.so bin/
