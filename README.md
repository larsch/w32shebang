# w32shebang

A command line script loader for Shell, Perl Python, Ruby and other
launguage on the Windows command line.

## Usage

Copy and rename w32shebang.exe to <your-script-name>.exe and put it next
to your script. When you run "<your-script-name>", it will find and
launch the interpreter for the script.

## How it works

First w32shebang looks for a script with the same name as the
executable with any of the PATHEXT extensions. If found, it checks for
a associated program and runs it if found.

If none is found, it tries any of the "known" script types (.sh, .pl,
.py, .rb) and looks for known interpreter on the PATH (sh, bash, perl,
python, ruby) and runs it if found.

The interpreter is run as a child process, and w32shebang waits and
return the exit code of the interpreter. The script name is passed as
the first argument, followed by any of the original arguments passed
to the executable.

Standard input, output and error output is inherited by the interpret
and can be piped to other programs or files.

Control-C and Control-Break is ignored by w32shebang, but will be
signalled to the interpreter. If it exits, w32shebang will exit.

## Examples

```dos
demo.sh
demo.exe (copy of w32shebang.exe)

C:\example>set PATHEXT
PATHEXT=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC

C:\example>where sh
c:\Program Files\Git\usr\bin\sh.exe

C:\example>cat demo.sh | demo arg1 "arg number 2"
arg1: arg1
arg2: arg number 2
input: echo arg1: $1
input: echo arg2: $2
input: while read n; do
input: echo input: $n
input: done
input: exit 1
```

## Background

Window doesn't support executable bits and 'shebang' program
loader. Various hacks have been used to mimic the unix-like behaviour.

* .bat or .cmd loader scripts result in ``Terminate batch job (Y/N)?``
  prompts.

* Renaming scripts to .rb, .py, .sh etc, adding them to PATHEXT and
  settings file associations sort of works, but you can't pipe into or
  out of a program launched though a file association.
