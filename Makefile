CC = clang++
CFLAGS = -c -g -O3 
LDFLAGS = `llvm-config --cxxflags`

all:
	@$(CC) *.cc $(CFLAGS) 
	@$(CC) -o interpreter *.o $(LDFLAGS)

clean:
	@rm *.o
	@rm interpreter
