el.so: el.o
	gcc -o el.so -shared el.o -llua -ldl -lm -ledit -L/opt/local/lib

el.o: el.c
	gcc -c el.c -fPIC -I/opt/local/include
clean:
	rm -f *.o *.so
