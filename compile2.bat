@echo off
setlocal enabledelayedexpansion

:: Compile 64-bit DLL
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

del *.obj 2>nul

set "SOURCES="
for /f "tokens=*" %%i in ('dir /b cmds\*.c') do (
    set "SOURCES=!SOURCES! cmds\%%i"
)

set "SOURCES=!SOURCES! SliverToolbox.c"

cl /c /O2 /W4 /TP /I. /Icmds %SOURCES%
lib /out:SliverToolbox64.lib *.obj
link /DLL /DEF:SliverToolbox64.def /OUT:SliverToolbox64.dll *.obj Ws2_32.lib WsmSvc.lib
cl /O2 /W4 /TC /I. test_harness.c /link SliverToolbox64.lib

:: Compile 32-bit DLL
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86

del *.obj 2>nul

set "SOURCES="
for /f "tokens=*" %%i in ('dir /b cmds\*.c') do (
    set "SOURCES=!SOURCES! cmds\%%i"
)

set "SOURCES=!SOURCES! SliverToolbox.c"

cl /c /O2 /W4 /TP /I. /Icmds %SOURCES%
lib /out:SliverToolbox32.lib *.obj
link /DLL /DEF:SliverToolbox32.def /OUT:SliverToolbox32.dll *.obj Ws2_32.lib WsmSvc.lib


copy SliverToolbox32.dll C:\share\SliverToolbox\SliverToolbox32.dll
copy SliverToolbox64.dll C:\share\SliverToolbox\SliverToolbox64.dll
copy extension.json C:\share\SliverToolbox\extension.json