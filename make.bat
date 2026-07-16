@echo off
gcc.exe -m32 -O1 Orca.c -o Orca.exe -lComctl32 -lComdlg32 -lgdi32 -luser32
pause