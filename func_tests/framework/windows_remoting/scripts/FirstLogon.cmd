@echo Wait until Network Adapters are not in Identified... state anymore.
@timeout 10
powershell -NonInteractive -Command "Set-NetConnectionProfile -NetworkCategory Private"
@echo Wait until settings take effect. Immediate execution of "winrm quickconfig -quiet" fails.
@timeout 10
call winrm quickconfig -quiet
@pause
