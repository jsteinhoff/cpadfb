CFLAGS = -O3 -pipe

all:	cpadfb

.PHONY: clean

clean:
	$(RM) -f *.o *~ cpadfb
