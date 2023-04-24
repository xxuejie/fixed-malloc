CC := riscv64-ckb-elf-gcc
AR := riscv64-ckb-elf-ar
LIB := libfixedmalloc.a
CFLAGS := -std=c99 -nostdlib -Wall -Werror -Wextra -Wno-unused-parameter -Wno-dangling-pointer -Wno-nonnull -Wno-nonnull-compare -fno-builtin-printf -fno-builtin-memcmp -O3 -g -fdata-sections -ffunction-sections

default: $(LIB)

fmt:
	clang-format-15 -i --style=Google *-malloc.c *-malloc.h utils.h

$(LIB): linear-malloc.o slab-malloc.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(LIB) *.o

.PHONY: clean default fmt
