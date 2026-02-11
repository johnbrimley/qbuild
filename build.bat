@echo off
echo Building qbuild...
c:\tc\bin\tcc -c -ms -nbuild\obj -obuild\obj\qbuild.obj qbuild.c > build\log\build.log
c:\tc\bin\tlink c:\tc\lib\c0s.obj build\obj\qbuild.obj, build\bin\qbuild.exe, build\log\qbuild.map, c:\tc\lib\cs.lib >> build\log\build.log
echo Building qbuild done.
