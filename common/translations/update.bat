@echo off
lupdate -pro ..\x86\common.pro -pluralonly -ts common_en.ts
lconvert -i common_en.ts -i common_en.ts.inc -o common_en.ts
lupdate -pro ..\x86\common.pro -ts common_fr.ts common_ru.ts common_zh-CN.ts common_jp.ts common_ko.ts common_pt-BR.ts
