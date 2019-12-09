@echo off
call ${environment.dir}/python/x86/Scripts/virtualenv ./x86/python
call ${environment.dir}/python/x64/Scripts/virtualenv ./x64/python

echo Replace distutils for cx_Freeze to work
call rmdir /s /q x86\python\Lib\distutils
call mkdir x86\python\Lib\distutils
call xcopy ${environment.dir}\python\x86\Lib\distutils x86\python\Lib\distutils /s /e
call rmdir /s /q x64\python\Lib\distutils
call mkdir x64\python\Lib\distutils
call xcopy ${environment.dir}\python\x64\Lib\distutils x64\python\Lib\distutils /s /e