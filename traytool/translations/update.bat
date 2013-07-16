@echo off
lupdate -pro ..\x86\traytool.pro -pluralonly -ts traytool_en.ts 
lconvert -i traytool_en.ts -i traytool_en.ts.inc -o traytool_en.ts
lupdate -pro ..\x86\traytool.pro -ts traytool_fr.ts traytool_ru.ts traytool_zh-CN.ts
