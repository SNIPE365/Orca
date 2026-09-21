@echo off
cd %~dp0
path=g:\mingw32\bin;%path%;
gcc.exe -m32 -O1 Orca.c -o Orca.exe -lComctl32 -lComdlg32 -lgdi32 -luser32 -lwinmm
pause
