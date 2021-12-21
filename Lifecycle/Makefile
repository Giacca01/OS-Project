# dal carattere '#' fino a fine riga, il testo dentro il Makefile e`
# un commento
#
# flags per la compilazione
CFLAGS = -std=c89 -pedantic 

# con "make clean" solitamente si vogliono eliminare i prodotti della
# compilazione e ripristinare il contenuto originale
#all: master

clean:
	rm -f *.o master *~

master: master.c Makefile
	gcc $(CFLAGS) master.c -o master


# il target run si usa talvolta per eseguire l'applicazione
run:  master
	./master

