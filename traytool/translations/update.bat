@echo off
lupdate -pro ..\x64\traytool.pro -locations absolute -pluralonly -ts traytool_en.ts 
lupdate -pro ..\x64\traytool.pro -locations absolute -ts traytool_fr.ts traytool_ru.ts traytool_zh-CN.ts traytool_jp.ts traytool_ko.ts traytool_pt-BR.ts traytool_es.ts
