@echo off
lupdate -pro ..\x86\client.pro -pluralonly -ts client_en.ts 
lconvert -i client_en.ts -i client_en.ts.inc -o client_en.ts
lupdate -pro ..\x86\client.pro -ts client_fr.ts client_ru.ts client_zh-CN.ts
