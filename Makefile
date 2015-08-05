
all:fastrm

fastrm:fastrm.o
	${CC} ${CCFLAGS} -o fastrm fastrm.c

install:fastrm
	install fastrm /usr/local/bin

dist:fastrm
	scp -r ../fastrm root2@web00.education.com:src
	ssh root2@web00.education.com 'cd src/fastrm;make clean install;cd /usr/local/bin;dist fastrm'
	scp run-fastrm root2@web00.education.com:/usr/local/bin
	ssh root2@web00.education.com 'cd /usr/local/bin;dist run-fastrm'

clean:
	rm -f fastrm *.o *~
