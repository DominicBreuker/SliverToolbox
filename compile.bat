@echo off
setlocal enabledelayedexpansion

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

del *.obj 2>nul
del *.dll 2>nul
del *.exe 2>nul

set "CSOURCES="

for /f "tokens=*" %%i in ('dir /b cmds\*.c') do (
    set "CSOURCES=!CSOURCES! cmds\%%i"
)

set "CSOURCES=!CSOURCES! SliverToolbox.c"

cl /c /O2 /W4 /TP /I. /Icmds %CSOURCES%
lib /out:SliverToolbox.lib *.obj
link /DLL /DEF:SliverToolbox.def /OUT:SliverToolbox.dll *.obj Ws2_32.lib WsmSvc.lib
cl /O2 /W4 /TC /I. test_harness.c /link SliverToolbox.lib 
