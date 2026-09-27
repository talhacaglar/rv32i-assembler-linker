# Makefile - RV32I Assembler + Linker
# Kullanim: make all

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99 -I src/common -I src/assembler -I src/linker
OUTDIR  = output

ASM_SRCS = src/common/utils.c src/assembler/encoder.c src/assembler/parser.c src/assembler/main.c
LNK_SRCS = src/linker/core.c src/linker/parser.c src/linker/relocator.c src/linker/writer.c src/linker/main.c

all: assembler linker

assembler: $(ASM_SRCS)
	$(CC) $(CFLAGS) -o assembler_bin $(ASM_SRCS)
	@echo "[OK] assembler derlendi"

linker: $(LNK_SRCS)
	$(CC) $(CFLAGS) -o linker_bin $(LNK_SRCS)
	@echo "[OK] linker derlendi"

build: all
	@mkdir -p $(OUTDIR)
	@cd test_programs && \
	../assembler_bin main.s utils.s && \
	../linker_bin main.o utils.o \
		-o ../$(OUTDIR)/knight_rider \
		--text-base 00000000 \
		--data-base 00000300 \
		--stack-top 00000400
	@echo ""
	@echo "=== Cikti dosyalari ==="
	@cat $(OUTDIR)/knight_rider.map

test: all
	python3 tests/run_tests.py
	python3 -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -f assembler_bin linker_bin
	rm -f test_programs/*.o tests/test_programs/*.o gui/temp_main.o
	@if [ -d "$(OUTDIR)" ]; then find "$(OUTDIR)" -type f -delete; fi

.PHONY: all assembler linker build test clean
