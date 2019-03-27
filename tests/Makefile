.RECIPEPREFIX +=

TESTS := simple

CFLAGS = -O2 -std=gnu99 -MMD -MP -g
CFLAGS += -I../include/

# Enable as many warnings as possible and treat them as errors
CFLAGS += -Werror -Wall -Wextra

# Disable lazy runtime binding so we can optimize libraries
LDFLAGS = -Wl,-z,now

SRC = simple.c
DEP = $(SRC:.c=.d)

.PHONY: all
all: $(TESTS)

simple: simple.o ../libdrob.so
    $(CC) $(LDFLAGS) -o $@ $<  -L.. -ldrob

%.o: %.c
    $(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
    rm -f *.d *.o $(TESTS)

-include $(DEP)
