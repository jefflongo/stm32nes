# File locations
SRCDIR = src
IDIR = src/include
ODIR = build

# Includes
_DEPS = bitmask.h cartridge.h cpu.h nes.h ppu.h types.h mappers/mapper0.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

# Object files
_OBJ = cartridge.o cpu.o ppu.o mapper0.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

# Compile flags
CC = gcc
CFLAGS = -Wall -I$(IDIR) -I$(IDIR)/mappers

# Debug flags
DBGCFLAGS = -g -O0

all: main

# $(ODIR)/%.o: %.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/cartridge.o: $(SRCDIR)/cartridge.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

$(ODIR)/cpu.o: $(SRCDIR)/cpu.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

$(ODIR)/ppu.o: $(SRCDIR)/ppu.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

$(ODIR)/mapper0.o: $(SRCDIR)/mappers/mapper0.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

$(ODIR)/main.o: $(SRCDIR)/main.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

$(ODIR)/test.o: $(SRCDIR)/test.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

main: $(OBJ) $(ODIR)/main.o 
	$(CC) -o $@ $^ $(CFLAGS) $(DBGCFLAGS)

test: $(OBJ) $(ODIR)/test.o
	$(CC) -o $@ $^ $(CFLAGS) $(DBGCFLAGS)

clean:
	del $(ODIR)/*.o *.exe