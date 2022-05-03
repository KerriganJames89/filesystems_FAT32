project3: main.o
	gcc -o project3.exe main.o

main.o: main.c fat.h
	gcc -c main.c

clean:
	rm *.o project3.exe	