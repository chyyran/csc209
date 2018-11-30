CFLAGS = -Wall -g

helpcentre: helpcentre.o hcq.o dynstr.o
	gcc $(CFLAGS) -o helpcentre helpcentre.o hcq.o dynstr.o

helpcentre.o: helpcentre.c hcq.h
	gcc $(CFLAGS) -c helpcentre.c

hcq.o: hcq.c hcq.h dynstr.h
	gcc $(CFLAGS) -c hcq.c

dynstr.o: dynstr.c dynstr.h
	gcc $(CFLAGS) -c dynstr.c

clean: 
	rm helpcentre *.o
