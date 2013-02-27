compiler=g++
source=test0.cpp
target=test0
cflags=`pkg-config opencv --cflags --libs`


objects = $(source:.cpp=.o)

$(target): $(objects)
	@$(compiler) $(cflags) $(objects) -o $@

.cpp.o: $<
	@echo Compiling $<
	@$(compiler) -c $(cflags) $< -o $@

clean: 
	@rm -f $(objects) $(target)

