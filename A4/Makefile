PORT=56704
CFLAGS= -DPORT=\$(PORT) -g -Wall

hcq_server: hcq_server.o hcq.o dynstr.o client.o panic.o
	gcc $(CFLAGS) -o hcq_server hcq_server.o hcq.o dynstr.o client.o panic.o

hcq_server.o: hcq_server.c hcq.h client.h
	gcc $(CFLAGS) -c hcq_server.c

helpcentre: helpcentre.o hcq.o dynstr.o
	gcc $(CFLAGS) -o helpcentre helpcentre.o hcq.o dynstr.o
	
helpcentre.o: helpcentre.c hcq.h
	gcc $(CFLAGS) -c helpcentre.c

hcq.o: hcq.c hcq.h dynstr.h
	gcc $(CFLAGS) -c hcq.c

dynstr.o: dynstr.c dynstr.h panic.h
	gcc $(CFLAGS) -c dynstr.c

client.o: client.c client.h dynstr.h hcq.h
	gcc $(CFLAGS) -c client.c

panic.o: panic.c panic.h
	gcc $(CFLAGS) -c panic.c

clean: 
	rm helpcentre hcq_server *.o
