LDFLAGS=-lusb-1.0 -llo -lpthread 

default: 
	clang ${LDFLAGS} -o linome src/linome.c 

clean:
	rm linome

