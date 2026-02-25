# ===== Compiler =====
CC = gcc

# ===== Flags =====
CFLAGS = -Wall -Wextra -O2 `sdl2-config --cflags`
LIBS = `sdl2-config --libs` -lSDL2_ttf -lm

# ===== Target name =====
TARGET = subtractive_synth

# ===== Source =====
SRC = subtractive_synth.c

# ===== Build =====
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# ===== Run =====
run: $(TARGET)
	./$(TARGET)

# ===== Clean =====
clean:
	rm -f $(TARGET)