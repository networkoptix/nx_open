@echo off
lupdate -pro ..\x86\common.pro -pluralonly -ts common_en.ts
lupdate -pro ..\x86\common.pro -ts common_fr.ts common_ru.ts common_zh-CN.ts
