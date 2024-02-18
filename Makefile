

SRC=

LIB=fconf.c

test1:
	$(CC) -g3 -Wall $(SRC) $(LIB) $@.c -o $@

test2:
	$(CC) -g3 -Wall $(SRC) $(LIB) $@.c -o $@

test3:
	$(CC) -g3 -Wall $(SRC) $(LIB) $@.c -o $@

clean:
