@echo off
setlocal enabledelayedexpansion

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

cl /c /O2 /W4 /TP /I. move_winrm.c 
link /OUT:move_winrm.exe move_winrm.obj ws2_32.lib WsmSvc.lib

.\move_winrm.exe %*
