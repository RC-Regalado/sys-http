INCLUDES = -I./src -Isrc/include
FLAGS = -Wall -Wextra -Wpedantic -O2 -g -fPIC -MMD -MP $(INCLUDES)
LIB = ./bin/libmicrodb.so
BIN = ./bin/microdb

LIB_FILES = ./build/crc32.o ./build/db.o ./build/hash.o ./build/util.o ./build/wal.o ./build/mmap.o ./build/page.o ./build/paged_file.o
BIN_FILES = ./build/main.o
DEP_FILES = $(LIB_FILES:.o=.d) $(BIN_FILES:.o=.d)

all: $(LIB) $(BIN)

test: $(LIB) $(BIN)
	sh ./tests/recovery.sh
	sh ./tests/compaction.sh
	sh ./tests/metadata.sh

$(LIB): $(LIB_FILES)
	gcc -shared -Wl,-soname,libmicrodb.so -o $@ $(LIB_FILES)

$(BIN): $(BIN_FILES) $(LIB)
	gcc -Wall -Wextra -Wpedantic -O2 -g -o $@ $(BIN_FILES) -L./bin -lmicrodb -Wl,-rpath,'$$ORIGIN'

build/%.o: src/asm/%.s
	as -o $@ $<

build/%.o: src/%.c
	gcc $(FLAGS) -c $< -o $@

clean:
			@rm -rf ./bin/microdb
			@rm -rf ./bin/libmicrodb.so
			@rm -rf $(LIB_FILES) $(BIN_FILES) $(DEP_FILES)

-include $(DEP_FILES)
