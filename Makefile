# Define the compiler and flags
CC = gcc
CFLAGS = -Wall

# Define the target executable
TARGET = dragonshell

# Target to compile the source code without creating a .dSYM folder
$(TARGET): dragonshell.c
	$(CC) $(CFLAGS) -o $(TARGET) dragonshell.c

# Clean up the compiled files, including removing any .dSYM folder if it exists
clean:
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM
