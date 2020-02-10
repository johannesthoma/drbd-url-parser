CFLAGS=-Wall -g -I. -DUSER_MODE=1

all: parser

clean:
	rm -f parser
