@echo off
lupdate -pro ..\x64\client.pro -locations absolute -pluralonly -ts client_en.ts 
lupdate -pro ..\x64\client.pro -locations absolute -ts client_fr.ts client_ru.ts client_zh-CN.ts client_jp.ts client_ko.ts client_pt-BR.ts client_es.ts
