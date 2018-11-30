CFLAGS = -Wall -g


hcq_server: hcq_server.o hcq.o dynstr.o srvman.o
	gcc $(CFLAGS) -o hcq_server hcq_server.o hcq.o dynstr.o srvman.o

hcq_server.o: hcq_server.c hcq.h srvman.h
	gcc $(CFLAGS) -c hcq_server.c

helpcentre: helpcentre.o hcq.o dynstr.o
	gcc $(CFLAGS) -o helpcentre helpcentre.o hcq.o dynstr.o
	
helpcentre.o: helpcentre.c hcq.h
	gcc $(CFLAGS) -c helpcentre.c

hcq.o: hcq.c hcq.h dynstr.h
	gcc $(CFLAGS) -c hcq.c

dynstr.o: dynstr.c dynstr.h
	gcc $(CFLAGS) -c dynstr.c

srvman.o: srvman.c srvman.h
	gcc $(CFLAGS) -c srvman.c

clean: 
	rm helpcentre *.o
