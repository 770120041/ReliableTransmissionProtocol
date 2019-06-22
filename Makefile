all: reliable_sender reliable_receiver

reliable_sender:
	gcc -o reliable_sender my_sender.c

reliable_receiver:
	gcc -o reliable_receiver my_receiver.c

clean:
	@rm -f reliable_sender reliable_receiver *.o
