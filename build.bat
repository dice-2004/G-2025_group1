@echo off
REM Gomoku AI Build Script for Windows
REM ====================================

echo Building Gomoku AI...
echo.

REM Clean previous build
if exist gomoku.exe del gomoku.exe
if exist *.o del *.o
if exist logics\*.o del logics\*.o

echo [1/10] Compiling config.c...
gcc -Wall -O3 -c config.c -o config.o
if errorlevel 1 goto error

echo [2/10] Compiling gomoku_ai.c...
gcc -Wall -O3 -c gomoku_ai.c -o gomoku_ai.o
if errorlevel 1 goto error

echo [3/10] Compiling logics/evaluation.c...
gcc -Wall -O3 -c logics\evaluation.c -o logics\evaluation.o
if errorlevel 1 goto error

echo [4/10] Compiling logics/transposition.c...
gcc -Wall -O3 -c logics\transposition.c -o logics\transposition.o
if errorlevel 1 goto error

echo [5/10] Compiling logics/opening_book.c...
gcc -Wall -O3 -c logics\opening_book.c -o logics\opening_book.o
if errorlevel 1 goto error

echo [6/10] Compiling logics/learning.c...
gcc -Wall -O3 -c logics\learning.c -o logics\learning.o
if errorlevel 1 goto error

echo [7/10] Compiling logics/main_engine.c...
gcc -Wall -O3 -c logics\main_engine.c -o logics\main_engine.o
if errorlevel 1 goto error

echo [8/10] Compiling logics/TSS.c...
gcc -Wall -O3 -c logics\TSS.c -o logics\TSS.o
if errorlevel 1 goto error

echo [9/12] Compiling logics/debug_utils.c...
gcc -Wall -O3 -c logics\debug_utils.c -o logics\debug_utils.o
if errorlevel 1 goto error

echo [10/12] Compiling .libgomoku.c (for gomoku.c)...
gcc -Wall -O3 -c .libgomoku.c -o .libgomoku.o
if errorlevel 1 goto error

echo [11/12] Compiling .libgomoku_to.c (for AI - no logs)...
gcc -Wall -O3 -c .libgomoku_to.c -o .libgomoku_to.o
if errorlevel 1 goto error

echo [12/12] Compiling gomoku.c and linking...
gcc -Wall -O3 -c gomoku.c -o gomoku.o
if errorlevel 1 goto error

gcc gomoku.o gomoku_ai.o config.o .libgomoku.o .libgomoku_to.o logics\evaluation.o logics\transposition.o logics\opening_book.o logics\learning.o logics\main_engine.o logics\TSS.o logics\debug_utils.o -o gomoku_new.exe
if errorlevel 1 goto error

echo.
echo ========================================
echo Build completed successfully!
echo Executable: gomoku_new.exe
echo ========================================
echo.
echo Run with: gomoku_new.exe
goto end

:error
echo.
echo ========================================
echo Build FAILED!
echo ========================================
exit /b 1

:end
