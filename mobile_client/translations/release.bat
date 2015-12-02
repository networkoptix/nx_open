@echo off
for %%i in (*.ts) do (
	lrelease "%%i"
)
