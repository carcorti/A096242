CC ?= gcc
SRC := a096242.c
ROOT := ..
BIN := $(ROOT)/build/a096242
BFILE := $(ROOT)/data/b096242.txt
TMPDIR := $(ROOT)/tmp

.DELETE_ON_ERROR:

COMMON_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -pthread
RELEASE_CFLAGS := -O3 -march=native -flto -DNDEBUG
DEBUG_CFLAGS := -O0 -g3
SAN_CFLAGS := -O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
SAN_LDFLAGS := -fsanitize=address,undefined
LDFLAGS ?=
RELEASE_LDFLAGS := -flto

.PHONY: all release debug sanitize test test-segmented test-auto test-boundary test-validation-input test-invalid-size test-bfile-boundary test-determinism clean

all: release

release: $(BIN)

$(BIN): $(SRC) | build
	$(CC) $(COMMON_CFLAGS) $(RELEASE_CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS) $(RELEASE_LDFLAGS)

debug: $(BIN)_debug

$(BIN)_debug: $(SRC) | build
	$(CC) $(COMMON_CFLAGS) $(DEBUG_CFLAGS) $(SRC) -o $@ $(LDFLAGS)

sanitize: $(BIN)_san

$(BIN)_san: $(SRC) | build
	$(CC) $(COMMON_CFLAGS) $(SAN_CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(SAN_LDFLAGS)

build:
	mkdir -p $(ROOT)/build

tmp:
	mkdir -p $(TMPDIR)

levels:
	mkdir -p $(ROOT)/levels

test: release tmp
	$(BIN) --max-n 10 --threads 1 --mode memory --validate --bfile $(BFILE) --tmp-dir $(TMPDIR)

test-segmented: release tmp
	$(BIN) --max-n 10 --threads 4 --mode segmented --chunk-parents 10000 --validate --bfile $(BFILE) --tmp-dir $(TMPDIR)

test-auto: release tmp
	$(BIN) --max-n 10 --threads 4 --mode auto --validate --bfile $(BFILE) --tmp-dir $(TMPDIR)

test-boundary: release tmp
	! $(BIN) --max-n 21 --threads 1 --mode memory --tmp-dir $(TMPDIR)

test-validation-input: release tmp
	printf '# comment\n1 4\n2 14\n' > $(TMPDIR)/b096242_comment_test.txt
	$(BIN) --max-n 2 --threads 1 --mode memory --validate --bfile $(TMPDIR)/b096242_comment_test.txt --tmp-dir $(TMPDIR)
	printf '# comment only\n' > $(TMPDIR)/b096242_empty_test.txt
	! $(BIN) --max-n 2 --threads 1 --mode memory --validate --bfile $(TMPDIR)/b096242_empty_test.txt --tmp-dir $(TMPDIR)

test-invalid-size: release tmp
	! $(BIN) --max-n 2 --threads 1 --mode segmented --chunk-parents -1 --validate --bfile $(BFILE) --tmp-dir $(TMPDIR)
	! $(BIN) --max-n 2 --memory-gb nan --validate --bfile $(BFILE) --tmp-dir $(TMPDIR)
	! $(BIN) --max-n

test-bfile-boundary: release tmp
	printf '1 4\n255 99991\n' > $(TMPDIR)/b096242_index255_test.txt
	$(BIN) --max-n 1 --validate --bfile $(TMPDIR)/b096242_index255_test.txt --tmp-dir $(TMPDIR)
	printf '256 99991\n' > $(TMPDIR)/b096242_index256_test.txt
	! $(BIN) --max-n 1 --validate --bfile $(TMPDIR)/b096242_index256_test.txt --tmp-dir $(TMPDIR)

test-determinism: release tmp
	$(RM) -r $(TMPDIR)/det_t1 $(TMPDIR)/det_t4
	$(BIN) --max-n 10 --threads 1 --mode memory --validate --bfile $(BFILE) --tmp-dir $(TMPDIR) --save-levels $(TMPDIR)/det_t1
	$(BIN) --max-n 10 --threads 4 --mode memory --validate --bfile $(BFILE) --tmp-dir $(TMPDIR) --save-levels $(TMPDIR)/det_t4
	cmp $(TMPDIR)/det_t1/D10.bin $(TMPDIR)/det_t4/D10.bin

clean:
	$(RM) -r $(ROOT)/build $(TMPDIR)/det_t1 $(TMPDIR)/det_t4 $(ROOT)/tmp/*.bin $(ROOT)/levels/*.bin
