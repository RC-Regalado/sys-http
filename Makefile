FILES = ./build/main.o ./build/io.o ./build/str.o ./build/hashmap.o ./build/mmap.o ./build/memory.o ./build/server.o ./build/requests.o ./build/epoll_loop.o ./build/client.o ./build/files.o ./build/syscalls.o ./build/json.o ./build/database.o ./build/query.o ./build/handlers.o
DEP_FILES = $(FILES:.o=.d)
INCLUDES = -I./src -Isrc/includes -Imicrodb/src/include/
# -MMD -MP: genera build/%.d por cada .o compilado, con las dependencias de
# headers de ese .c. Sin esto, `make` (incremental) no sabe que un .o depende
# de un .h y no lo recompila al editar solo el header -- eso produce binarios
# donde distintos .o quedan compilados contra versiones distintas del mismo
# struct (ABI mismatch entre translation units), causando corrupcion de
# memoria intermitente dificil de diagnosticar (offsets de struct
# desalineados entre quien escribe y quien lee el mismo campo).
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -fno-stack-protector -fvisibility=hidden -MMD -MP
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
		@rm -rf ${DEP_FILES}
		@$(MAKE) -C microdb clean

.PHONY: microdb-lib
microdb-lib:
		@$(MAKE) -C microdb

$(MICROLIB): microdb-lib
		@cp microdb/bin/libmicrodb.so bin/

-include $(DEP_FILES)
