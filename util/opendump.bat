@ECHO OFF
SET DUMPFILE=%1

IF EXIST temp.txt DEL temp.txt

REM This needs to be invoked to be able to pass FOR tokens to external outside-of-the-loop variables (fecking bat) to retrieve buildnumber, branch etc.
SETLOCAL ENABLEDELAYEDEXPANSION

IF NOT "%DUMPFILE%" == "" (
    goto :UNPACK 
) ELSE (
    SET /p DUMMY = No dump file specified. Press any key to exit...
)    
:UNPACK
    IF EXIST %DUMPFILE% (
        REM Finding build number
        FOR /F "tokens=5-8 delims=.-" %%I IN ("%DUMPFILE%") DO (
            SET BUILDNUMBER=%%I
            SET CUSTOMIZATION=%%K
        )    
        ECHO Build Number = !BUILDNUMBER!
        ECHO Customization = !CUSTOMIZATION!
        
        wget -qO - http://beta.networkoptix.com/beta-builds/daily/ | findstr !BUILDNUMBER! > temp.txt
        FOR /F "tokens=2 delims=><" %%I IN (temp.txt) DO SET BUILDBRANCH=%%I
        IF "!BUILDBRANCH!" == "" SET /p DUMMY = The build does not exist on the server ot Internet connection error occurs. Press any key to exit...
        ECHO Build Branch = !BUILDBRANCH!
        
        wget -qO - http://beta.networkoptix.com/beta-builds/daily/!BUILDBRANCH!/!CUSTOMIZATION!/windows/ | findstr !BUILDNUMBER! | findstr server-only | findstr x64 > temp.txt 
        FOR /F delims^=^"^ tokens^=2 %%I IN (temp.txt) DO SET INSTALLER_FILENAME=%%I
        IF "!INSTALLER_FILENAME!" == "" SET /p DUMMY = The Windows Installer does not exist on the server ot Internet connection error occurs. Press any key to exit...
        ECHO Installer Filename = !INSTALLER_FILENAME!        
        
        wget -qO - http://beta.networkoptix.com/beta-builds/daily/!BUILDBRANCH!/!CUSTOMIZATION!/updates/!BUILDNUMBER! | findstr pdb-all > temp.txt  
        FOR /F delims^=^"^ tokens^=2 %%I IN (temp.txt) DO SET PDBALL_FILENAME=%%I
        IF "!PDBALL_FILENAME!" == "" SET /p DUMMY = The Windows Installer does not exist on the server ot Internet connection error occurs. Press any key to exit...
        ECHO PDB All Filename = !PDBALL_FILENAME!

        wget -qO - http://beta.networkoptix.com/beta-builds/daily/!BUILDBRANCH!/!CUSTOMIZATION!/updates/!BUILDNUMBER! | findstr pdb-apps > temp.txt  
        FOR /F delims^=^"^ tokens^=2 %%I IN (temp.txt) DO SET PDBAPPS_FILENAME=%%I
        IF "!PDBAPPS_FILENAME!" == "" SET /p DUMMY = The Windows Installer does not exist on the server ot Internet connection error occurs. Press any key to exit...
        ECHO PDB Zip Filename = !PDBAPPS_FILENAME!
        
        ECHO Downloading installer...
        IF EXIST !INSTALLER_FILENAME! DEL !INSTALLER_FILENAME!
        wget http://beta.networkoptix.com/beta-builds/daily/!BUILDBRANCH!/!CUSTOMIZATION!/windows/!INSTALLER_FILENAME!
        ECHO Downloading PDB All...
        IF EXIST !PDBALL_FILENAME! DEL !PDBALL_FILENAME!
        wget http://beta.networkoptix.com/beta-builds/daily/!BUILDBRANCH!/!CUSTOMIZATION!/updates/!BUILDNUMBER!/!PDBALL_FILENAME!
        ECHO Downloading PDB Apps...
        IF EXIST !PDBAPPS_FILENAME! DEL !PDBAPPS_FILENAME!
        wget http://beta.networkoptix.com/beta-builds/daily/!BUILDBRANCH!/!CUSTOMIZATION!/updates/!BUILDNUMBER!/!PDBAPPS_FILENAME!
        
        ECHO Unpacking Installer...
        msiexec /a !INSTALLER_FILENAME! /qb TARGETDIR="%cd%\!BUILDNUMBER!"
        
        REM DIR /s /b mediaserver.exe > temp.txt
        REM SET /p MEDIASERVER_DIR=<temp.txt
        REM FOR /f %%i in ('DIR /s /b mediaserver.exe') do set MEDIASERVER_DIR=%%i
        FOR /r %%a in (.) DO @IF EXIST "%%~fa\mediaserver.exe" set MEDIASERVER_DIR=%%~fa
        ECHO Media Server folder is "!MEDIASERVER_DIR!"
        
        ECHO Copying and extracting Files...
        copy %DUMPFILE% "!MEDIASERVER_DIR!"
        7z x !PDBALL_FILENAME! -o"!MEDIASERVER_DIR!" -y
        7z x !PDBAPPS_FILENAME! -o"!MEDIASERVER_DIR!" -y
        7z x !PDBAPPS_FILENAME! -o"!MEDIASERVER_DIR!"-y
    ) ELSE (
        SET /p DUMMY = Dump file does not exist in this subdirectory. Press any key to exit...       
    )   