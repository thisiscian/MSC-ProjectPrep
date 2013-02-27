CC=g++
EXECUTABLE=voronoi
SOURCE=voronoi.cpp
OUTPUT=$(EXECUTABLE).pgm


run: program
	./$(EXECUTABLE) > $(OUTPUT)

program: $(SOURCE)
	$(CC) -o $(EXECUTABLE) $(SOURCE)

clean:
	rm $(EXECUTABLE) $(OUTPUT)
