# G2 エンジン (新エンジン) のビルド
#   make            -> build/pbrain-g2 (Gomocup エンジン), build/g2-test (テストツール)
#   make test       -> 差分更新の自己検証と、Python レフェリーとの禁じ手判定照合
CC ?= gcc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
BUILD = build

CORE = src/rule.c src/pattern.c src/board.c src/think.c
HDRS = $(wildcard src/*.h)

all: $(BUILD)/pbrain-g2 $(BUILD)/g2-test

$(BUILD)/pbrain-g2: $(CORE) src/gomocup.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CORE) src/gomocup.c -o $@

$(BUILD)/g2-test: $(CORE) tests/g2_test.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CORE) tests/g2_test.c -o $@

test: all
	$(BUILD)/g2-test selftest 4
	$(BUILD)/g2-test selftest 0
	$(BUILD)/g2-test selftest 1
	python3 tests/compare_forbidden.py --positions 300

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
