CC=g++
LIB=`pkg-config opencv --cflags --libs`

main: test0 test1 test2 test3

test0: test0.cpp
	$(CC) $(LIB) -o test0 test0.cpp

test1: test1.cpp
	$(CC) $(LIB) -o test1 test1.cpp

test2: test2.cpp
	$(CC) $(LIB) -o test2 test2.cpp

test3: SURF_Homography.cpp
	$(CC) $(LIB) -o test3 SURF_Homography.cpp

clean: 
	rm test0 test1 test2
