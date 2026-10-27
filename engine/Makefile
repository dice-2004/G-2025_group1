# G2 エンジン (新エンジン) のビルド
#   make            -> build/pbrain-g2 (Gomocup エンジン), build/g2-test (テストツール)
#   make test       -> 差分更新の自己検証と、Python レフェリーとの禁じ手判定照合、開局ブックの表引き照合
CC ?= gcc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -pthread
BUILD = build

CORE = $(filter-out src/gomocup.c src/class_entry.c,$(wildcard src/*.c))
HDRS = $(wildcard src/*.h)

all: $(BUILD)/pbrain-g2 $(BUILD)/g2-test $(BUILD)/g2-tss $(BUILD)/g2-book

$(BUILD)/pbrain-g2: $(CORE) src/gomocup.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CORE) src/gomocup.c -o $@

$(BUILD)/g2-test: $(CORE) tests/g2_test.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CORE) tests/g2_test.c -o $@

$(BUILD)/g2-tss: $(CORE) tests/tss_test.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CORE) tests/tss_test.c -o $@

$(BUILD)/g2-book: $(CORE) tests/book_test.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CORE) tests/book_test.c -o $@

test: all
	$(BUILD)/g2-test selftest 4
	$(BUILD)/g2-test selftest 0
	$(BUILD)/g2-test selftest 1
	python3 tests/compare_forbidden.py --positions 300
	$(BUILD)/g2-tss puzzles tests/puzzles/tactics.txt
	python3 book/check_book.py

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
