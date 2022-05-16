CFLAGS ?= -g 
CC ?=gcc

.PHONY: clean  all 

all: webbench 

webbench: webbench.c
	${CC} ${CFLAGS} $^ -o $@


clean:
	-rm -rf ./*.o webbench

