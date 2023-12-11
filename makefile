CFLAGS = -c -g -Wall -Werror
TARGETS = sysprak-client

$(TARGETS): sysprak-client.o think.o config.o fetch.o performConnection.o
	gcc -o $@ $^
%.o: %.c
	gcc $(CFLAGS) $<
clean:
	rm $(TARGETS)