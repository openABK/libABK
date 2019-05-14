echo Making 7z of openABK source files

REM Make a 7z file for later deploy in a temporary directory
set SEVENZ_EXE_PATH="C:\Program Files\7-Zip\7z.exe"
set ZIPFILE="c:\Temp\openABKSources.7z"
set ABKDIR="X:\Entwicklung\SW\Visualisierung\Abk"
del %ZIPFILE%

rem call %SEVENZ_EXE_PATH% a -t7z %ZIPFILE% %ABKDIR%\Common\*.cpp
rem IF ERRORLEVEL 1 GOTO ErrorOccured
rem call %SEVENZ_EXE_PATH% a -t7z %ZIPFILE% %ABKDIR%\Common\*.h
rem IF ERRORLEVEL 1 GOTO ErrorOccured

call %SEVENZ_EXE_PATH% a -t7z %ZIPFILE% %ABKDIR%\*.dummy -i!Common\*.cpp -i!Common\*.h -i!Common\*.txt -i!Server\*.cpp -i!Server\*.h -i!Client\*.cpp -i!Client\*.h -i!Discovery\*.cpp -i!Discovery\*.h -i!Server\Browser\*.*



COLOR a
ECHO Archive was sucessfully generated
Goto Done
:ErrorOccured
COLOR c
ECHO ### Error Occured! ###
:DONE

pause
