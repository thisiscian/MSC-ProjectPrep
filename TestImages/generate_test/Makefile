CC=g++
LIB=`pkg-config opencv --cflags --libs`
prog=generate_test

main: $(prog)

$(prog): $(prog).cpp
	$(CC) $(LIB) -o $(prog) $(prog).cpp

clean: 
	rm $(prog)
