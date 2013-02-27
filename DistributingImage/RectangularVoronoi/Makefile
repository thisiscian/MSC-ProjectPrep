CC=g++
EXECUTABLE=rectangularvoronoi
SOURCE=rectangularvoronoi.cpp
OUTPUT=$(EXECUTABLE).pgm


run: program
	./$(EXECUTABLE) > $(OUTPUT)

program: $(SOURCE)
	$(CC) -o $(EXECUTABLE) $(SOURCE)

clean:
	rm $(EXECUTABLE) $(OUTPUT)
