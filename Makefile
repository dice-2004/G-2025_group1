# Makefile for Gomoku AI Project v3.3

# プログラム名
PROGRAM = gomoku

# オブジェクトファイル（v3.3 完全版）
OBJS = .libgomoku.o gomoku.o gomoku_ai.o config.o .libgomoku_to.o\
       logics/evaluation.o logics/transposition.o logics/opening_book.o \
       logics/learning.o logics/main_engine.o logics/TSS.o logics/debug_utils.o

# コンパイラ設定
CC = gcc
CFLAGS = -Wall -O2 -std=c99
LDFLAGS = -lm

# AI 本体 (審判 .libgomoku.c / gomoku.c を除いた部分)。ベンチ用アダプタと共有する
AI_OBJS = gomoku_ai.o config.o .libgomoku_to.o \
       logics/evaluation.o logics/transposition.o logics/opening_book.o \
       logics/learning.o logics/main_engine.o logics/TSS.o logics/debug_utils.o

# デフォルトターゲット
all: $(PROGRAM)

# 新エンジン G2 (engine/)。Gomocup 版は engine/build/pbrain-g2
.PHONY: engine g2class
engine:
	$(MAKE) -C engine

# 授業の枠組み (gomoku.c, .libgomoku.c は無変更) に G2 を組み込んだ実行ファイル
G2_CORE = $(filter-out engine/src/gomocup.c engine/src/class_entry.c,$(wildcard engine/src/*.c))
g2class: gomoku_g2
gomoku_g2: gomoku.c .libgomoku.c gomoku.h $(G2_CORE) engine/src/class_entry.c $(wildcard engine/src/*.h)
	$(CC) -O2 -std=c11 -Wall -D_POSIX_C_SOURCE=200809L gomoku.c .libgomoku.c $(G2_CORE) engine/src/class_entry.c $(LDFLAGS) -pthread -o $@

# ベンチマーク用 Gomocup エンジン (bench/README.md 参照)
.PHONY: bench
bench: bench/build/pbrain-legacy

bench/build/pbrain-legacy: bench/pbrain_adapter.c gomoku.h $(AI_OBJS)
	@mkdir -p bench/build
	$(CC) $(CFLAGS) bench/pbrain_adapter.c $(AI_OBJS) $(LDFLAGS) -o $@

# 実行ファイル生成
$(PROGRAM): $(OBJS)
	@echo "Linking $(PROGRAM)..."
	$(CC) $(OBJS) $(LDFLAGS) -o $(PROGRAM)
	@echo "Build completed successfully!"
	@echo "Executable: $(PROGRAM)"

# 個別コンパイルルール
.libgomoku.o: .libgomoku.c gomoku.h
	@echo "Compiling .libgomoku.c..."
	$(CC) $(CFLAGS) -c .libgomoku.c -o .libgomoku.o

.libgomoku_to.o: .libgomoku_to.c gomoku.h
	@echo "Compiling .libgomoku_to.c..."
	$(CC) $(CFLAGS) -c .libgomoku_to.c -o .libgomoku_to.o

gomoku.o: gomoku.c gomoku.h
	@echo "Compiling gomoku.c..."
	$(CC) $(CFLAGS) -c gomoku.c -o gomoku.o

gomoku_ai.o: gomoku_ai.c gomoku_ai.h gomoku.h config.h
	@echo "Compiling gomoku_ai.c..."
	$(CC) $(CFLAGS) -c gomoku_ai.c -o gomoku_ai.o

config.o: config.c config.h logics/learning.h
	@echo "Compiling config.c..."
	$(CC) $(CFLAGS) -c config.c -o config.o

logics/evaluation.o: logics/evaluation.c logics/evaluation.h gomoku.h gomoku_ai.h
	@echo "Compiling logics/evaluation.c..."
	$(CC) $(CFLAGS) -c logics/evaluation.c -o logics/evaluation.o

logics/transposition.o: logics/transposition.c logics/transposition.h gomoku.h gomoku_ai.h
	@echo "Compiling logics/transposition.c..."
	$(CC) $(CFLAGS) -c logics/transposition.c -o logics/transposition.o

logics/opening_book.o: logics/opening_book.c logics/opening_book.h gomoku.h gomoku_ai.h
	@echo "Compiling logics/opening_book.c..."
	$(CC) $(CFLAGS) -c logics/opening_book.c -o logics/opening_book.o

logics/learning.o: logics/learning.c logics/learning.h gomoku.h gomoku_ai.h
	@echo "Compiling logics/learning.c..."
	$(CC) $(CFLAGS) -c logics/learning.c -o logics/learning.o

logics/main_engine.o: logics/main_engine.c logics/main_engine.h gomoku.h gomoku_ai.h config.h
	@echo "Compiling logics/main_engine.c..."
	$(CC) $(CFLAGS) -c logics/main_engine.c -o logics/main_engine.o

logics/TSS.o: logics/TSS.c logics/TSS.h gomoku.h gomoku_ai.h
	@echo "Compiling logics/TSS.c..."
	$(CC) $(CFLAGS) -c logics/TSS.c -o logics/TSS.o

logics/debug_utils.o: logics/debug_utils.c logics/debug_utils.h gomoku.h gomoku_ai.h config.h
	@echo "Compiling logics/debug_utils.c..."
	$(CC) $(CFLAGS) -c logics/debug_utils.c -o logics/debug_utils.o

# クリーンアップ
.PHONY: clean
clean:
	@echo "Cleaning up..."
	$(RM) $(PROGRAM) $(OBJS)
	$(RM) gomoku.exe *.o
	$(RM) logics/*.o
	$(RM) -r bench/build gomoku_g2
	$(MAKE) -C engine clean
	@echo "Clean completed."

# 完全クリーン（学習データも削除）
.PHONY: distclean
distclean: clean
	@echo "Removing learning data..."
	$(RM) learning_data.dat
	@echo "Full clean completed."

# リビルド
.PHONY: rebuild
rebuild: clean all

# ヘルプ
.PHONY: help
help:
	@echo "Gomoku AI Build System v3.3"
	@echo "============================"
	@echo "Usage:"
	@echo "  make all       - Build the program"
	@echo "  make clean     - Remove all build artifacts"
	@echo "  make distclean - Remove build artifacts and learning data"
	@echo "  make rebuild   - Clean and rebuild"
	@echo "  make bench     - Build Gomocup adapter for bench/ (bench/build/pbrain-legacy)"
	@echo "  make help      - Show this help message"
	@echo ""
	@echo "Features:"
	@echo "  - Learning system (PRACTICE/PRODUCTION mode)"
	@echo "  - Iterative Deepening with time management"
	@echo "  - Debug utilities (DEBUG_LEVEL 0-3)"
	@echo "  - Configurable via config.txt"
