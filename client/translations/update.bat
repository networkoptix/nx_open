@echo off
lupdate -pro ..\x64\client.pro -locations absolute -pluralonly -ts client_en.ts 
lupdate -pro ..\x64\client.pro -locations absolute -ts client_es.ts client_fr.ts client_ja.ts client_ko.ts client_pt_BR.ts client_ru.ts client_zh_CN.ts client_zh_TW.ts
