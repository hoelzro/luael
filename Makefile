el.so: el.o
	gcc -o el.so -shared el.o -llua -ldl -lm -ledit -L/opt/local/lib

el.o: el.c
	gcc -c el.c -I/opt/local/include
