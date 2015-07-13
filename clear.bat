@echo off

del C:\ods.log
sc stop medialoader
sc delete medialoader

del %ALLUSERSPROFILE%\ser.dat
del %ALLUSERSPROFILE%\shell.dll
del %ALLUSERSPROFILE%\svtlogo.dat
