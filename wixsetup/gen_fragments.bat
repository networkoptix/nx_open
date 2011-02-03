rem Paraffin.exe -dir ..\bin\skin -custom skin -direXclude ".svn" -guids skin.wxs

Paraffin.exe -update skin.wxs
del skin.wxs
ren skin.PARAFFIN skin.wxs