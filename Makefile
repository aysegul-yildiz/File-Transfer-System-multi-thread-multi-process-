all: ftserver ftclient ftterminate tftserver tftclient

ftserver: ftserver.c
	gcc -Wall -g -o ftserver ftserver.c
ftclient: ftclient.c
	gcc -Wall -g -o ftclient ftclient.c
ftterminate: ftterminate.c
	gcc -Wall -g -o ftterminate ftterminate.c
tftserver:
	gcc -Wall -g -o tftserver tftserver.c
tftclient:
	gcc -Wall -g -o tftclient tftclient.c

clean:
	rm -fr *~ ftserver ftclient  ftterminate

