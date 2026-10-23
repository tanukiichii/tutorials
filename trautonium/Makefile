# ===== Compiler =====
CC = gcc

# ===== Flags =====
CFLAGS = -Wall -Wextra -O2 `sdl2-config --cflags`
LIBS = `sdl2-config --libs` -lSDL2_ttf -lm

# ===== Target name =====
TARGET = trautonium

# ===== Source =====
SRC = trautonium_line_control.c #true_subtractive_synth.c

# ===== Build =====
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# ===== Run =====
run: $(TARGET)
	./$(TARGET)

# ===== Clean =====
clean:
	rm -f $(TARGET)