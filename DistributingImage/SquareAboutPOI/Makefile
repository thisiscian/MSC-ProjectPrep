CC=g++
EXECUTABLE=squareaboutpoi
SOURCE=squareaboutpoi.cpp
OUTPUT=$(EXECUTABLE).pgm


run: program
	./$(EXECUTABLE) > $(OUTPUT)

program: $(SOURCE)
	$(CC) -o $(EXECUTABLE) $(SOURCE)

clean:
	rm $(EXECUTABLE) $(OUTPUT)
