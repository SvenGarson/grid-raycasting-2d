# OBJS specified which files are to be compiled
OBJS = source/*.c

# CC specifies the compiler to use
CC = gcc

# COMPILER_FLAGS specifies additional compiler flags
COMPILER_FLAGS = -Wall -Wextra -std=c99

# LINKER_FLAGS specifies the libraries to link against
LINKER_FLAGS = -lSDL2 -lGL -lGLU -lm

# OBJ_NAME specifies the name of the executable
OBJ_NAME = builds/driver

# Putting everything together for compilation
all : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)
