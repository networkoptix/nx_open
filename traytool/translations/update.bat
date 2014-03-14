@echo off
lupdate -pro ..\x64\traytool.pro -locations absolute -pluralonly -ts traytool_en.ts 
lupdate -pro ..\x64\traytool.pro -locations absolute -ts traytool_es.ts traytool_fr.ts traytool_ja.ts traytool_ko.ts traytool_pt_BR.ts traytool_ru.ts traytool_zh_CN.ts traytool_zh_TW.ts
