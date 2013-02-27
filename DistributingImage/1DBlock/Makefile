CC=g++
EXECUTABLE=block1d
SOURCE=block1d.cpp
OUTPUT=$(EXECUTABLE).pgm


run: program
	./$(EXECUTABLE) > $(OUTPUT)

program: $(SOURCE)
	$(CC) -o $(EXECUTABLE) $(SOURCE)

clean:
	rm $(EXECUTABLE) $(OUTPUT)
