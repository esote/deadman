PROG=	deadman
SRCS=	deadman.c

CFLAGS=		-O2 -fstack-protector -D_FORTIFY_SOURCE=2 -pie -fPIE
LDFLAGS=	-Wl,-z,now -Wl,-z,relro

$(PROG): $(SRCS)
	gcc $(CFLAGS) $(LDFLAGS) -o $(PROG).out $(SRCS)

debug: $(SRCS)
	gcc -g -Wall -Wextra -Wconversion -o $(PROG).out $(SRCS)

clean:
	rm -f $(PROG).out
