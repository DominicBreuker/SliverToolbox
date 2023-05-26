# SliverToolbx

Just a small playground for Sliver extension development.
Work in progress...

## How to use

You can compile with `combile.bat` (on my VM).

The extension may one day be a small collection of tools with a bit of shared code (argument parsing and stuff).
For testing you can use `test_harness.exe` and try the commands.
For example, run
- `.\test_harness.exe portscan hosts:localhost ports:80,443,445` to test for open ports.
- `.\test_harness.exe move-winrm host:10.11.12.13 cmd:whoami username:Administrator password:password` to run a command with WinRM

## Developing new commands

You can add one in `SliverToolbox.c` in the same way the existing ones are there.
It parses all the args for you, provided you follow the `key:value` syntax.
You command can then use them.
Also make sure your command uses the `append` function from the toolbox for output printing.
Return 0 from your command if everything is all right, else not 0.


